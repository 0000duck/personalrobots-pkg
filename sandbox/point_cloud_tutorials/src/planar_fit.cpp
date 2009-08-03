/*
 * Copyright (c) 2009 Radu Bogdan Rusu <rusu -=- cs.tum.edu>
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

@b planar_fit attempts to fit the best plane to a given PointCloud message, using several different techniques.
This node should be used for testing different planar segmentation strategies.

**/

// ROS core
#include <ros/node.h>
// ROS messages
#include <robot_msgs/PointCloud.h>
#include <robot_msgs/PointStamped.h>
#include <mapping_msgs/PolygonalMap.h>

// For the normals visualization
#include <visualization_msgs/Marker.h>

#include <tf/transform_listener.h>

// Cloud kd-tree
#include <point_cloud_mapping/kdtree/kdtree_ann.h>

// Cloud geometry
#include <point_cloud_mapping/geometry/angles.h>
#include <point_cloud_mapping/geometry/point.h>
#include <point_cloud_mapping/geometry/areas.h>
#include <point_cloud_mapping/geometry/nearest.h>
#include <point_cloud_mapping/geometry/intersections.h>

// Sample Consensus
#include <point_cloud_mapping/sample_consensus/sac.h>
#include <point_cloud_mapping/sample_consensus/ransac.h>
#include <point_cloud_mapping/sample_consensus/rransac.h>
#include <point_cloud_mapping/sample_consensus/msac.h>
#include <point_cloud_mapping/sample_consensus/rmsac.h>
#include <point_cloud_mapping/sample_consensus/sac_model_plane.h>

#include <angles/angles.h>

using namespace std;
using namespace robot_msgs;

class PlanarFit
{
protected:
	ros::Node& node_;

public:

	// ROS messages
	PointCloud cloud_, cloud_down_, cloud_plane_, cloud_outliers_;


	tf::TransformListener tf_;

	// Kd-tree stuff
	cloud_kdtree::KdTree *kdtree_;
	vector<vector<int> > points_indices_;

	// Parameters
	double radius_;
	int k_;

	// Additional downsampling parameters
	int downsample_;
	Point leaf_width_;

	vector<cloud_geometry::Leaf> leaves_;

	// Do not use normals (0), use radius search (0) or use K-nearest (1) for point neighbors
	int radius_or_knn_;

	// If normals_fidelity_, then use the original cloud data to estimate the k-neighborhood and thus the normals
	int normals_fidelity_;

	double sac_distance_threshold_;
	int sac_maximum_iterations_;

	// Euclidean clustering
	int use_clustering_;      // use clustering (1) or not (0)
	double euclidean_cluster_angle_tolerance_, euclidean_cluster_distance_tolerance_;
	int euclidean_cluster_min_pts_;

	// Normal line markers
	int publish_normal_lines_as_markers_;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	PlanarFit (ros::Node& anode) : node_ (anode),  tf_ (anode)
	{
		node_.param ("~search_radius_or_knn", radius_or_knn_, 1);   // none (0), radius (1) or k (2) nearest neighbor search
		node_.param ("~search_radius", radius_, 0.01);              // 1cm radius by default
		node_.param ("~search_k_closest", k_, 20);                  // 20 k-neighbors by default

		node_.param ("~downsample", downsample_, 1);                        // downsample cloud before normal estimation
		node_.param ("~downsample_leaf_width_x", leaf_width_.x, 0.02);      // 2cm radius by default
		node_.param ("~downsample_leaf_width_y", leaf_width_.y, 0.02);      // 2cm radius by default
		node_.param ("~downsample_leaf_width_z", leaf_width_.z, 0.02);      // 2cm radius by default

		node_.param ("~normals_high_fidelity", normals_fidelity_, 1);       // compute the downsampled normals from the original data

		node_.param ("~sac_distance_threshold", sac_distance_threshold_, 0.02);   // 2 cm threshold
		node_.param ("~sac_maximum_iterations", sac_maximum_iterations_, 500);    // maximum 500 SAC iterations

		node_.param ("~use_clustering", use_clustering_, 0);
		node_.param ("~euclidean_cluster_angle_tolerance", euclidean_cluster_angle_tolerance_, 15.0);
		euclidean_cluster_angle_tolerance_ = angles::from_degrees (euclidean_cluster_angle_tolerance_);
		node_.param ("~euclidean_cluster_min_pts", euclidean_cluster_min_pts_, 500);
		node_.param ("~euclidean_cluster_distance_tolerance", euclidean_cluster_distance_tolerance_, 0.05);

		node_.param ("~publish_normal_lines_as_markers", publish_normal_lines_as_markers_, 1);

		string cloud_topic ("tilt_laser_cloud");

		vector<pair<string, string> > t_list;
		node_.getPublishedTopics (&t_list);
		bool topic_found = false;
		for (vector<pair<string, string> >::iterator it = t_list.begin (); it != t_list.end (); it++)
		{
			if (it->first.find (node_.resolveName (cloud_topic)) != string::npos)
			{
				topic_found = true;
				break;
			}
		}
		if (!topic_found)
			ROS_WARN ("Trying to subscribe to %s, but the topic doesn't exist!", node_.resolveName (cloud_topic).c_str ());

		node_.subscribe (cloud_topic, cloud_, &PlanarFit::cloud_cb, this, 1);
		node_.advertise<PointCloud> ("~normals", 1);
		node_.advertise<PointCloud> ("~plane", 1);
		node_.advertise<PointCloud> ("~outliers", 1);
		node_.advertise<mapping_msgs::PolygonalMap>("~convex_hull",1);
		// A channel to visualize the normals as cute little lines
		//node_.advertise<PolyLine> ("~normal_lines", 1);
		node_.advertise<visualization_msgs::Marker>( "visualization_marker", 0 );
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	virtual ~PlanarFit () { }

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/** \brief Get the view point from where the scans were taken in the incoming PointCloud message frame
	* \param cloud_frame the point cloud message TF frame
	* \param viewpoint_cloud the resultant view point in the incoming cloud frame
	* \param tf a pointer to a TransformListener object
	*/
	void
	getCloudViewPoint (const string &cloud_frame, PointStamped &viewpoint_cloud, tf::TransformListener &tf)
	{
		// Figure out the viewpoint value in the point cloud frame
		PointStamped viewpoint_laser;
		viewpoint_laser.header.frame_id = "laser_tilt_mount_link";
		// Set the viewpoint in the laser coordinate system to 0, 0, 0
		viewpoint_laser.point.x = viewpoint_laser.point.y = viewpoint_laser.point.z = 0.0;

		try
		{
			tf.transformPoint (cloud_frame, viewpoint_laser, viewpoint_cloud);
			ROS_INFO ("Cloud view point in frame %s is: %g, %g, %g.", cloud_frame.c_str (),
					viewpoint_cloud.point.x, viewpoint_cloud.point.y, viewpoint_cloud.point.z);
		}
		catch (...)
		{
			// Default to 0.05, 0, 0.942768 (base_link, ~base_footprint)
			viewpoint_cloud.point.x = 0.05; viewpoint_cloud.point.y = 0.0; viewpoint_cloud.point.z = 0.942768;
			ROS_WARN ("Could not transform a point from frame %s to frame %s! Defaulting to <%f, %f, %f>",
					viewpoint_laser.header.frame_id.c_str (), cloud_frame.c_str (),
					viewpoint_cloud.point.x, viewpoint_cloud.point.y, viewpoint_cloud.point.z);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	updateParametersFromServer ()
	{
		if (node_.hasParam ("~search_radius_or_knn")) node_.getParam ("~search_radius_or_knn", radius_or_knn_);
		if (node_.hasParam ("~search_radius")) node_.getParam ("~search_radius", radius_);
		if (node_.hasParam ("~search_k_closest")) node_.getParam ("~search_k_closest", k_);

		if (node_.hasParam ("~downsample")) node_.getParam ("~downsample", downsample_);
		if (node_.hasParam ("~downsample_leaf_width_x")) node_.getParam ("~downsample_leaf_width_x", leaf_width_.x);
		if (node_.hasParam ("~downsample_leaf_width_y")) node_.getParam ("~downsample_leaf_width_y", leaf_width_.y);
		if (node_.hasParam ("~downsample_leaf_width_z")) node_.getParam ("~downsample_leaf_width_z", leaf_width_.z);

		if (node_.hasParam ("~normals_high_fidelity")) node_.getParam ("~normals_high_fidelity", normals_fidelity_);

		if (node_.hasParam ("~sac_distance_threshold")) node_.getParam ("~sac_distance_threshold", sac_distance_threshold_);
		if (node_.hasParam ("~sac_maximum_iterations")) node_.getParam ("~sac_maximum_iterations", sac_maximum_iterations_);

		if (node_.hasParam ("~use_clustering")) node_.getParam ("~use_clustering", use_clustering_);
		if (node_.hasParam ("~euclidean_cluster_min_pts")) node_.getParam ("~euclidean_cluster_min_pts", euclidean_cluster_min_pts_);
		if (node_.hasParam ("~euclidean_cluster_distance_tolerance")) node_.getParam ("~euclidean_cluster_distance_tolerance", euclidean_cluster_distance_tolerance_);
		if (node_.hasParam ("~euclidean_cluster_angle_tolerance"))
		{
			node_.getParam ("~euclidean_cluster_angle_tolerance", euclidean_cluster_angle_tolerance_);
			euclidean_cluster_angle_tolerance_ = angles::from_degrees (euclidean_cluster_angle_tolerance_);
		}
		if (node_.hasParam ("~publish_normal_lines_as_markers")) node_.getParam ("~publish_normal_lines_as_markers", publish_normal_lines_as_markers_);
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Callback
	void cloud_cb ()
	{
		updateParametersFromServer ();

		ROS_INFO ("Received %d data points in frame %s with %d channels (%s).", (int)cloud_.pts.size (), cloud_.header.frame_id.c_str (),
				(int)cloud_.chan.size (), cloud_geometry::getAvailableChannels (cloud_).c_str ());
		if (cloud_.pts.size () == 0)
		{
			ROS_ERROR ("No data points found. Exiting...");
			return;
		}

		cloud_down_.header = cloud_.header;
		cloud_down_.chan.clear();

		//ROS_INFO ("Original: %d channels, Downsampled: %d channels", cloud_.chan.size(), cloud_down_.chan.size());

		ros::Time ts = ros::Time::now ();
		// Figure out the viewpoint value in the cloud_frame frame
		PointStamped viewpoint_cloud;
		getCloudViewPoint (cloud_.header.frame_id, viewpoint_cloud, tf_);

		// ---------------------------------------------------------------------------------------------------------------
		// ---[ Downsample and/or estimate point normals
		if (downsample_ != 0)
		{
			int d_idx = cloud_geometry::getChannelIndex (cloud_, "distances");

			ros::Time ts1 = ros::Time::now ();
			try
			{
				// We sacrifice functionality for speed. Use a fixed 3D grid to downsample the data instead of an octree structure
				cloud_geometry::downsamplePointCloud (cloud_, cloud_down_, leaf_width_, leaves_, d_idx, DBL_MAX);  // -1 means use all data
			}
			catch (std::bad_alloc)
			{
				ROS_ERROR ("Not enough memory to create a simple downsampled representation. Change the downsample_leaf_width parameters.");
				return;
			}
			ROS_INFO ("Downsampling enabled. Number of points left: %d / %d in %g seconds.", (int)cloud_down_.pts.size (), (int)cloud_.pts.size (), (ros::Time::now () - ts1).toSec ());

			if (radius_or_knn_ == 1)             // Use a radius search
			{
				if (normals_fidelity_)
					cloud_geometry::nearest::computePointCloudNormals (cloud_down_, cloud_, radius_, viewpoint_cloud);  // Estimate point normals in the original point cloud using a fixed radius search
				else
					cloud_geometry::nearest::computePointCloudNormals (cloud_down_, radius_, viewpoint_cloud);          // Estimate point normals in the downsampled point cloud using a fixed radius search
			}
			else if (radius_or_knn_ == 2)        // Use a k-nearest neighbors search
			{
				if (normals_fidelity_)
					cloud_geometry::nearest::computePointCloudNormals (cloud_down_, cloud_, k_, viewpoint_cloud);       // Estimate point normals in the original point cloud using a K-nearest search
				else
					cloud_geometry::nearest::computePointCloudNormals (cloud_down_, k_, viewpoint_cloud);               // Estimate point normals in the downsampled point cloud using a K-nearest search
			}
			node_.publish ("~normals", cloud_down_);
			if (publish_normal_lines_as_markers_)
			{
				publishNormalLines (cloud_down_, 0, 1, 2, 0.01, 0.0005);
			}
		} // if (downsample)
		else
		{
			int first_normal_index = cloud_.chan.size();
			if (radius_or_knn_ == 1)             // Use a radius search
				cloud_geometry::nearest::computePointCloudNormals (cloud_, radius_, viewpoint_cloud);    // Estimate point normals using a fixed radius search
			else if (radius_or_knn_ == 2)        // Use a k-nearest neighbors search
				cloud_geometry::nearest::computePointCloudNormals (cloud_, k_, viewpoint_cloud);         // Estimate point normals using a K-nearest search
			node_.publish ("~normals", cloud_);
			if (publish_normal_lines_as_markers_)
			{
				publishNormalLines (cloud_, first_normal_index, first_normal_index+1, first_normal_index+2, 0.01, 0.0005);
			}
		}
		ROS_INFO ("Normals estimated in %g seconds.", (ros::Time::now () - ts).toSec ());


		// ---------------------------------------------------------------------------------------------------------------
		// ---[ Fit a planar model
		vector<int> inliers;
		vector<double> coeff;
		int total_nr_points;
		ts = ros::Time::now ();

		// find and publish convex hull
		mapping_msgs::PolygonalMap pmap;
		pmap.header.stamp = cloud_.header.stamp;
		pmap.header.frame_id = cloud_.header.frame_id;
		pmap.polygons.resize (1);

		if (downsample_ != 0)
		{
			total_nr_points = cloud_down_.pts.size ();

			// Break into clusters
			if (use_clustering_)
			{
				ros::Time ts1 = ros::Time::now ();
				vector<vector<int> > clusters;

				if (radius_or_knn_ != 0)                  // did we estimate normals ?
					cloud_geometry::nearest::extractEuclideanClusters (cloud_down_, euclidean_cluster_distance_tolerance_, clusters, 0, 1, 2, euclidean_cluster_angle_tolerance_, euclidean_cluster_min_pts_);
				else                                      // if not, set nx_idx to -1 and perform a pure Euclidean clustering
					cloud_geometry::nearest::extractEuclideanClusters (cloud_down_, euclidean_cluster_distance_tolerance_, clusters, -1, 1, 2, euclidean_cluster_angle_tolerance_, euclidean_cluster_min_pts_);
				ROS_INFO ("Clustering done. Number of clusters extracted: %d in %g seconds.", (int)clusters.size (), (ros::Time::now () - ts1).toSec ());

				ts = ros::Time::now ();
				fitSACPlane (&cloud_down_, clusters[clusters.size () - 1], inliers, coeff, viewpoint_cloud, sac_distance_threshold_);    // Fit the plane model
			}
			else
				fitSACPlane (&cloud_down_, inliers, coeff, viewpoint_cloud, sac_distance_threshold_);    // Fit the plane model

			cloud_geometry::getPointCloud (cloud_down_, inliers, cloud_plane_);
			cloud_geometry::getPointCloudOutside (cloud_down_, inliers, cloud_outliers_);
			cloud_geometry::areas::convexHull2D (cloud_down_, inliers, coeff, pmap.polygons[0]);

		}
		else
		{
			total_nr_points = cloud_.pts.size ();

			// Break into clusters
			if (use_clustering_)
			{
				ros::Time ts1 = ros::Time::now ();
				vector<vector<int> > clusters;

				if (radius_or_knn_ != 0)                  // did we estimate normals ?
					cloud_geometry::nearest::extractEuclideanClusters (cloud_, euclidean_cluster_distance_tolerance_, clusters, 0, 1, 2, euclidean_cluster_angle_tolerance_, euclidean_cluster_min_pts_);
				else                                      // if not, set nx_idx to -1 and perform a pure Euclidean clustering
					cloud_geometry::nearest::extractEuclideanClusters (cloud_, euclidean_cluster_distance_tolerance_, clusters, -1, 1, 2, euclidean_cluster_angle_tolerance_, euclidean_cluster_min_pts_);
				ROS_INFO ("Clustering done. Number of clusters extracted: %d in %g seconds.", (int)clusters.size (), (ros::Time::now () - ts1).toSec ());

				ts = ros::Time::now ();
				fitSACPlane (&cloud_, clusters[clusters.size () - 1], inliers, coeff, viewpoint_cloud, sac_distance_threshold_);    // Fit the plane model
			}
			else
				fitSACPlane (&cloud_, inliers, coeff, viewpoint_cloud, sac_distance_threshold_);       // Fit the plane model

			cloud_geometry::getPointCloud (cloud_, inliers, cloud_plane_);
			cloud_geometry::getPointCloudOutside (cloud_, inliers, cloud_outliers_);
			cloud_geometry::areas::convexHull2D (cloud_, inliers, coeff, pmap.polygons[0]);

		}
		ROS_INFO ("Planar model found with %d / %d inliers in %g seconds.\n", (int)inliers.size (), total_nr_points, (ros::Time::now () - ts).toSec ());

		node_.publish ("~plane", cloud_plane_);
		node_.publish ("~outliers", cloud_outliers_);


		node_.publish("~convex_hull",pmap);

	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/** \brief Find a plane model in a point cloud with SAmple Consensus methods
	* \param points the point cloud message
	* \param inliers the resultant planar inliers
	* \param coeff the resultant plane coefficients
	* \param viewpoint_cloud a point to the pose where the data was acquired from (for normal flipping)
	* \param dist_thresh the maximum allowed distance threshold of an inlier to the model
	* \param min_pts the minimum number of points allowed as inliers for a plane model
	*/
	bool
	fitSACPlane (PointCloud *points, vector<int> &inliers, vector<double> &coeff,
			const robot_msgs::PointStamped &viewpoint_cloud, double dist_thresh)
	{
		// Create and initialize the SAC model
		sample_consensus::SACModelPlane *model = new sample_consensus::SACModelPlane ();

		//      sample_consensus::SAC *sac             = new sample_consensus::RANSAC (model, dist_thresh);
		//      sample_consensus::SAC *sac             = new sample_consensus::RRANSAC (model, dist_thresh);
		//      reinterpret_cast<sample_consensus::RRANSAC*>(sac)->setFractionNrPretest (10);
		//      sample_consensus::SAC *sac             = new sample_consensus::RMSAC (model, dist_thresh);
		//      reinterpret_cast<sample_consensus::RMSAC*>(sac)->setFractionNrPretest (10);

		sample_consensus::SAC *sac             = new sample_consensus::MSAC (model, dist_thresh);
		sac->setMaxIterations (sac_maximum_iterations_);
		model->setDataSet (points);

		// Search for the best plane
		if (sac->computeModel (0))
		{
			sac->computeCoefficients (coeff);     // Compute the model coefficients
			sac->refineCoefficients (coeff);      // Refine them using least-squares
			model->selectWithinDistance (coeff, dist_thresh, inliers);

			cloud_geometry::angles::flipNormalTowardsViewpoint (coeff, points->pts.at (inliers[0]), viewpoint_cloud);

			//ROS_INFO ("Found a planar model supported by %d inliers: [%g, %g, %g, %g]", (int)inliers.size (),
			//           coeff[0], coeff[1], coeff[2], coeff[3]);

			// Project the inliers onto the model
			model->projectPointsInPlace (inliers, coeff);
		}
		else
		{
			ROS_ERROR ("Could not compute a plane model.");
			return (false);
		}

		delete sac;
		delete model;
		return (true);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/** \brief Find a plane model in a point cloud with SAmple Consensus methods
	* \param points the point cloud message
	* \param indices a subset of point indices to use
	* \param inliers the resultant planar inliers
	* \param coeff the resultant plane coefficients
	* \param viewpoint_cloud a point to the pose where the data was acquired from (for normal flipping)
	* \param dist_thresh the maximum allowed distance threshold of an inlier to the model
	* \param min_pts the minimum number of points allowed as inliers for a plane model
	*/
	bool
	fitSACPlane (PointCloud *points, vector<int> &indices, vector<int> &inliers, vector<double> &coeff,
			const robot_msgs::PointStamped &viewpoint_cloud, double dist_thresh)
	{
		// Create and initialize the SAC model
		sample_consensus::SACModelPlane *model = new sample_consensus::SACModelPlane ();

		//      sample_consensus::SAC *sac             = new sample_consensus::RANSAC (model, dist_thresh);
		//      sample_consensus::SAC *sac             = new sample_consensus::RRANSAC (model, dist_thresh);
		//      reinterpret_cast<sample_consensus::RRANSAC*>(sac)->setFractionNrPretest (10);
		//      sample_consensus::SAC *sac             = new sample_consensus::RMSAC (model, dist_thresh);
		//      reinterpret_cast<sample_consensus::RMSAC*>(sac)->setFractionNrPretest (10);

		sample_consensus::SAC *sac             = new sample_consensus::MSAC (model, dist_thresh);
		sac->setMaxIterations (sac_maximum_iterations_);
		model->setDataSet (points, indices);

		// Search for the best plane
		if (sac->computeModel (0))
		{
			sac->computeCoefficients (coeff);     // Compute the model coefficients
			sac->refineCoefficients (coeff);      // Refine them using least-squares
			model->selectWithinDistance (coeff, dist_thresh, inliers);

			cloud_geometry::angles::flipNormalTowardsViewpoint (coeff, points->pts.at (inliers[0]), viewpoint_cloud);

			//ROS_INFO ("Found a planar model supported by %d inliers: [%g, %g, %g, %g]", (int)inliers.size (),
			//           coeff[0], coeff[1], coeff[2], coeff[3]);

			// Project the inliers onto the model
			model->projectPointsInPlace (inliers, coeff);
		}
		else
		{
			ROS_ERROR ("Could not compute a plane model.");
			return (false);
		}

		delete sac;
		delete model;
		return (true);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/** \brief Publish the normals as cute little lines
	* \NOTE: assumes normalized point normals !
	* \param points pointer to the point cloud message
	* \param nx_idx the index of the x normal
	* \param ny_idx the index of the y normal
	* \param nz_idx the index of the z normal
	* \param length the length of the normal lines
	* \param width the width of the normal lines
	*/
	void
	publishNormalLines (PointCloud &points, int nx_idx, int ny_idx, int nz_idx, double length, double width)
	{
		visualization_msgs::Marker marker;
		marker.header.frame_id = "base_link";
		marker.header.stamp = ros::Time();
		marker.ns = "normal_lines_ns";
		marker.id = 0;
		marker.type = visualization_msgs::Marker::LINE_LIST;
		marker.action = visualization_msgs::Marker::ADD;
		marker.pose.position.x = 0;
		marker.pose.position.y = 0;
		marker.pose.position.z = 0;
		marker.pose.orientation.x = 0.0;
		marker.pose.orientation.y = 0.0;
		marker.pose.orientation.z = 0.0;
		marker.pose.orientation.w = 1.0;
		marker.scale.x = width;
		marker.scale.y = 1;
		marker.scale.z = 1;
		marker.color.a = 1.0;
		marker.color.r = 1.0;
		marker.color.g = 1.0;
		marker.color.b = 1.0;

		int nr_points = points.pts.size ();

		marker.points.resize (2 * nr_points);
		for (int i = 0; i < nr_points; i++)
		{
			marker.points[2*i].x = points.pts[i].x;
			marker.points[2*i].y = points.pts[i].y;
			marker.points[2*i].z = points.pts[i].z;
			marker.points[2*i+1].x = points.pts[i].x + points.chan[nx_idx].vals[i] * length;
			marker.points[2*i+1].y = points.pts[i].y + points.chan[ny_idx].vals[i] * length;
			marker.points[2*i+1].z = points.pts[i].z + points.chan[nz_idx].vals[i] * length;
		}

		node_.publish ("visualization_marker", marker);
		ROS_INFO ("Published %d normal lines", nr_points);
	}
};

/* ---[ */
int
main (int argc, char** argv)
{
	ros::init (argc, argv);

	ros::Node ros_node ("planar_fit");

	PlanarFit p (ros_node);
	ros_node.spin ();

	return (0);
}
/* ]--- */

