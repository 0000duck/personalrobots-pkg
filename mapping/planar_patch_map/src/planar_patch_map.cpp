/*
 * Copyright (c) 2008 Radu Bogdan Rusu <rusu -=- cs.tum.edu>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */

/**
@mainpage

@htmlinclude manifest.html

\author Radu Bogdan Rusu

@b planar_patch_map transforms a 3D point cloud into a polygonal planar patch map using Sample Consensus techniques and Octrees.

 **/

// ROS core
#include <ros/node.h>
// ROS messages
#include <robot_msgs/PointCloud.h>
#include <robot_msgs/Polygon3D.h>
#include <robot_msgs/PolygonalMap.h>

// Sample Consensus
#include <point_cloud_mapping/sample_consensus/sac.h>
#include <point_cloud_mapping/sample_consensus/msac.h>
#include <point_cloud_mapping/sample_consensus/ransac.h>
#include <point_cloud_mapping/sample_consensus/sac_model_plane.h>

// Cloud geometry
#include <point_cloud_mapping/geometry/point.h>
#include <point_cloud_mapping/geometry/areas.h>
#include <point_cloud_mapping/geometry/nearest.h>
#include <point_cloud_mapping/geometry/intersections.h>



// Cloud Octree
#include <point_cloud_mapping/cloud_octree.h>

#include <sys/time.h>

using namespace std;
using namespace robot_msgs;

class PlanarPatchMap
{
  protected:
    ros::Node& node_;
  public:

    // ROS messages
    PointCloud cloud_, cloud_f_;

    // Octree stuff
    cloud_octree::Octree *octree_;
    float leaf_width_;

    // Parameters
    int sac_min_points_per_cell_;
    double d_min_, d_max_;

    double min_polygon_area_;
    bool filter_by_area_;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    PlanarPatchMap (ros::Node& anode) : node_ (anode)
    {
      node_.param ("~sac_min_points_per_cell", sac_min_points_per_cell_, 10);

      node_.param ("~distance_min", d_min_, 0.10);
      node_.param ("~distance_max", d_max_, 10.0);

      node_.param ("~min_polygon_area", min_polygon_area_, -1.0);
      if(min_polygon_area_<0)
	{
	  filter_by_area_=false;
	  ROS_INFO ("Filtering by polygon area is disabled, as ~min_polygon_area is negative or not specified.");
	}
      else
	{
	  filter_by_area_=true;
	}

      string cloud_topic ("tilt_laser_cloud");

      vector<pair<string, string> > t_list;
      node_.getPublishedTopics (&t_list);
      for (vector<pair<string, string> >::iterator it = t_list.begin (); it != t_list.end (); it++)
      {
        if (it->first.find (cloud_topic) == string::npos)
          ROS_WARN ("Trying to subscribe to %s, but the topic doesn't exist!", cloud_topic.c_str ());
      }

      node_.subscribe (cloud_topic, cloud_, &PlanarPatchMap::cloud_cb, this, 1);

      node_.advertise<PolygonalMap> ("planar_map", 1);

      leaf_width_ = 0.15f;
      octree_ = new cloud_octree::Octree (0.0f, 0.0f, 0.0f, leaf_width_, leaf_width_, leaf_width_, 0);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~PlanarPatchMap () { }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
      fitSACPlane (PointCloud *points, cloud_octree::Octree *octree, cloud_octree::Leaf* leaf, Polygon3D &poly)
    {
      double dist_thresh = 0.02;
      vector<int> indices = leaf->getIndices ();
      if ((int)indices.size () < sac_min_points_per_cell_)
        return;

      // Create and initialize the SAC model
      sample_consensus::SACModelPlane *model = new sample_consensus::SACModelPlane ();
      sample_consensus::SAC *sac             = new sample_consensus::RANSAC (model, dist_thresh);
      model->setDataSet (points, indices);

      vector<int> inliers;
      vector<double> coeff;
      // Search for the best plane
      if (sac->computeModel ())
      {
        // Obtain the inliers and the planar model coefficients
        sac->computeCoefficients (coeff);
        sac->refineCoefficients (coeff);
        model->selectWithinDistance (coeff, dist_thresh, inliers);
        ///fprintf (stderr, "> Found a model supported by %d inliers: [%g, %g, %g, %g]\n", inliers.size (), coeff[0], coeff[1], coeff[2], coeff[3]);

        // Project the inliers onto the model
        model->projectPointsInPlace (inliers, coeff);

        cloud_geometry::areas::convexHull2D (*model->getCloud (), inliers, coeff, poly);
      }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
      filterCloudBasedOnDistance (robot_msgs::PointCloud *cloud_in, robot_msgs::PointCloud &cloud_out,
                                  int d_idx, double d_min, double d_max)
    {
      cloud_out.pts.resize (cloud_in->pts.size ());
      int nr_p = 0;

      for (unsigned int i = 0; i < cloud_out.pts.size (); i++)
      {
        if (cloud_in->chan[d_idx].vals[i] >= d_min && cloud_in->chan[d_idx].vals[i] <= d_max)
        {
          cloud_out.pts[nr_p] = cloud_in->pts[i];
          nr_p++;
        }
      }
      cloud_out.pts.resize (nr_p);
    }


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Callback
    void cloud_cb ()
    {
      ROS_INFO ("Received %d data points.", (int)cloud_.pts.size ());

      int d_idx = cloud_geometry::getChannelIndex (cloud_, "distances");
      if (d_idx != -1)
      {
        filterCloudBasedOnDistance (&cloud_, cloud_f_, d_idx, d_min_, d_max_);
        ROS_INFO ("Distance information present. Filtering points between %g and %g : %d / %d left.", d_min_, d_max_,
                  (int)cloud_f_.pts.size (), (int)cloud_.pts.size ());
      }
      else
        cloud_f_ = cloud_;

      timeval t1, t2;
      gettimeofday (&t1, NULL);

      octree_ = new cloud_octree::Octree (0.0f, 0.0f, 0.0f, leaf_width_, leaf_width_, leaf_width_, 0);
      // Insert all data points into the octree
      for (unsigned int i = 0; i < cloud_f_.pts.size (); i++)
        octree_->insert (cloud_f_.pts[i].x, cloud_f_.pts[i].y, cloud_f_.pts[i].z, i);

      gettimeofday (&t2, NULL);
      double time_spent = t2.tv_sec + (double)t2.tv_usec / 1000000.0 - (t1.tv_sec + (double)t1.tv_usec / 1000000.0);
      ROS_INFO ("Created octree with %d leaves (%d ndivs) in %g seconds.", octree_->getNumLeaves (), octree_->getNumCells (), time_spent);

      // Initialize the polygonal map
      PolygonalMap pmap;
      pmap.polygons.resize (octree_->getNumLeaves ());
      int nr_poly = 0;

      std::vector<cloud_octree::Leaf*> leaves = octree_->getOccupiedLeaves ();
      int total_pts = 0;

      gettimeofday (&t1, NULL);

      for (std::vector<cloud_octree::Leaf*>::iterator it = leaves.begin (); it != leaves.end (); ++it)
      {
        cloud_octree::Leaf* leaf = *it;
        total_pts += leaf->getIndices ().size ();

        fitSACPlane (&cloud_f_, octree_, leaf, pmap.polygons[nr_poly]);

	bool bFilter=false;

	if(filter_by_area_)
	  {
	    double polygon_area = cloud_geometry::areas::compute2DPolygonalArea(pmap.polygons[nr_poly]);
	    if( polygon_area < min_polygon_area_)
	      {
		bFilter=true;
	      }
	  }

	if(pmap.polygons[nr_poly].get_points_size()==0)
	  bFilter=true;

	//Increase the polygon count only for accepted polygons.
	if(~bFilter)
	  nr_poly++;
      }
      pmap.polygons.resize (nr_poly);

      gettimeofday (&t2, NULL);
      time_spent = t2.tv_sec + (double)t2.tv_usec / 1000000.0 - (t1.tv_sec + (double)t1.tv_usec / 1000000.0);
      ROS_INFO ("Number of: [total points / points inserted / difference] : [%d / %d / %d] in %g seconds.", (int)cloud_f_.pts.size (), total_pts, (int)fabs (cloud_f_.pts.size () - total_pts), time_spent);
      pmap.header = cloud_.header;
      node_.publish ("planar_map", pmap);

      delete octree_;
    }
};

/* ---[ */
int
  main (int argc, char** argv)
{
  ros::init (argc, argv);

  ros::Node ros_node ("planar_patch_map_node");

  PlanarPatchMap p (ros_node);
  ros_node.spin ();

  return (0);
}
/* ]--- */

