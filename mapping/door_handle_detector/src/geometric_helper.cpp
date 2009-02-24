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

/** \author Radu Bogdan Rusu */

#include "geometric_helper.h"

using namespace std;
using namespace robot_msgs;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Get a set of point indices between some specified bounds
  * \param points a pointer to the point cloud message
  * \param indices the resultant set of indices
  * \param door_req a door request service containing the X-Y bounds in frame_p1 and frame_p2
  * \param tf a pointer to a TransformListener object
  * \param parameter_frame the TF frame ID in which min_z_bounds and max_z_bounds are given
  * \param min_z_bounds restrict the minimum search bounds on Z to this value
  * \param max_z_bounds restrict the maximum search bounds on Z to this value
  * \param frame_multiplier multiply the ||frame_p1-frame_p2|| distance by this number to wrap all possible situations in X-Y
  */
void
  obtainCloudIndicesSet (robot_msgs::PointCloud *points, vector<int> &indices, door_handle_detector::DoorDetector::Request door_req,
                         tf::TransformListener *tf, std::string parameter_frame,
                         double min_z_bounds, double max_z_bounds, double frame_multiplier)
{
  // frames used
  string cloud_frame = points->header.frame_id;
  string door_frame   = door_req.door.header.frame_id;

  // Resize the resultant indices to accomodate all data
  indices.resize (points->pts.size ());

  // Transform the X-Y bounds from the door request service into the cloud TF frame
  tf::Stamped<Point32> frame_p1 (door_req.door.frame_p1, points->header.stamp, door_frame);
  tf::Stamped<Point32> frame_p2 (door_req.door.frame_p2, points->header.stamp, door_frame);
  transformPoint (tf, cloud_frame, frame_p1, frame_p1);
  transformPoint (tf, cloud_frame, frame_p2, frame_p2);

  ROS_INFO ("Start detecting door at points in frame %s [%g, %g, %g] -> [%g, %g, %g]",
            cloud_frame.c_str (), frame_p1.x, frame_p1.y, frame_p1.z, frame_p2.x, frame_p2.y, frame_p2.z);

  // Obtain the bounding box information in the reference frame of the laser scan
  Point32 min_bbox, max_bbox;

  if (frame_multiplier == -1)
  {
    ROS_INFO ("Door frame multiplier set to -1. Using the entire point cloud data.");
    // Use the complete bounds of the point cloud
    cloud_geometry::statistics::getMinMax (points, min_bbox, max_bbox);
    for (unsigned int i = 0; i < points->pts.size (); i++)
      indices[i] = i;
  }
  else
  {
    // Transform the minimum/maximum Z bounds parameters from frame parameter_frame to the cloud TF frame
    min_z_bounds = transformDoubleValueTF (min_z_bounds, parameter_frame, cloud_frame, points->header.stamp, tf);
    max_z_bounds = transformDoubleValueTF (max_z_bounds, parameter_frame, cloud_frame, points->header.stamp, tf);
    ROS_INFO ("Capping Z-search using the door_min_z_bounds/door_max_z_bounds parameters in frame %s: [%g / %g]",
              cloud_frame.c_str (), min_z_bounds, max_z_bounds);

    // Obtain the actual 3D bounds
    get3DBounds (&frame_p1, &frame_p2, min_bbox, max_bbox, min_z_bounds, max_z_bounds, frame_multiplier);

    int nr_p = 0;
    for (unsigned int i = 0; i < points->pts.size (); i++)
    {
      if ((points->pts[i].x >= min_bbox.x && points->pts[i].x <= max_bbox.x) &&
          (points->pts[i].y >= min_bbox.y && points->pts[i].y <= max_bbox.y) &&
          (points->pts[i].z >= min_bbox.z && points->pts[i].z <= max_bbox.z))
      {
        indices[nr_p] = i;
        nr_p++;
      }
    }
    indices.resize (nr_p);
  }


  ROS_INFO ("Number of points in bounds [%f,%f,%f] -> [%f,%f,%f]: %d.",
             min_bbox.x, min_bbox.y, min_bbox.z, max_bbox.x, max_bbox.y, max_bbox.z, indices.size ());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool
  checkDoorEdges (robot_msgs::Polygon3D *poly, robot_msgs::Point32 *z_axis, double min_height, double eps_angle,
                  double &door_frame1, double &door_frame2)
{
  // Compute the centroid of the polygon
  robot_msgs::Point32 centroid;
  cloud_geometry::nearest::computeCentroid (poly, centroid);

  // Divide into left and right
  std::vector<int> inliers_left, inliers_right;
  for (unsigned int i = 0; i < poly->points.size (); i++)
  {
    if (poly->points[i].x < centroid.x)
    {
      if (poly->points[i].y < centroid.y)
        inliers_left.push_back (i);
      else
        inliers_right.push_back (i);
    }
    else
    {
      if (poly->points[i].y < centroid.y)
        inliers_left.push_back (i);
      else
        inliers_right.push_back (i);
    }
  }
//  std::cerr << "Inliers: " << inliers_left.size () << " " << inliers_right.size () << " " << poly->points.size () << std::endl;

  door_frame1 = door_frame2 = 0.0;
  robot_msgs::Point32 line_dir;
  // Parse over all the <left> lines defined by the polygon and check their length
  std::vector<robot_msgs::Point32> new_points;
  for (unsigned int i = 0; i < inliers_left.size () - 1; i++)
  {
    // Check if the points are equal
    if (cloud_geometry::checkPointEqual (&poly->points.at (inliers_left[i]), &poly->points.at (inliers_left[i+1])))
      continue;
    // Check if there is a jump in the order of the points on the convex hull
    if (fabs (inliers_left[i] - inliers_left[i+1]) > 1)
      continue;

    // Get the line direction between points i and i+1
    line_dir.x = poly->points.at (inliers_left[i]).x - poly->points.at (inliers_left[i+1]).x;
    line_dir.y = poly->points.at (inliers_left[i]).y - poly->points.at (inliers_left[i+1]).y;
    line_dir.z = poly->points.at (inliers_left[i]).z - poly->points.at (inliers_left[i+1]).z;

    // Compute the angle between this direction and the Z axis
    double angle = cloud_geometry::angles::getAngle3D (&line_dir, z_axis);
    if ( (angle < eps_angle) || ( (M_PI - angle) < eps_angle ) )
    {
      // Compute the length of the line
      double line_length = cloud_geometry::distances::pointToPointDistance (&poly->points.at (inliers_left[i]), &poly->points.at (inliers_left[i+1]));
      door_frame1 += line_length;

      new_points.push_back (poly->points.at (inliers_left[i]));
      new_points.push_back (poly->points.at (inliers_left[i+1]));
    }
  }

  // Parse over all the <right> lines defined by the polygon and check their length
  for (unsigned int i = 0; i < inliers_right.size () - 1; i++)
  {
    // Check if the points are equal
    if (cloud_geometry::checkPointEqual (&poly->points.at (inliers_right[i]), &poly->points.at (inliers_right[i+1])))
      continue;
    // Check if there is a jump in the order of the points on the convex hull
    if (fabs (inliers_right[i] - inliers_right[i+1]) > 1)
      continue;
    // Get the line direction between points i and i+1
    line_dir.x = poly->points.at (inliers_right[i]).x - poly->points.at (inliers_right[i+1]).x;
    line_dir.y = poly->points.at (inliers_right[i]).y - poly->points.at (inliers_right[i+1]).y;
    line_dir.z = poly->points.at (inliers_right[i]).z - poly->points.at (inliers_right[i+1]).z;

    // Compute the angle between this direction and the Z axis
    double angle = cloud_geometry::angles::getAngle3D (&line_dir, z_axis);
    if ( (angle < eps_angle) || ( (M_PI - angle) < eps_angle ) )
    {
      // Compute the length of the line
      double line_length = cloud_geometry::distances::pointToPointDistance (&poly->points.at (inliers_right[i]), &poly->points.at (inliers_right[i+1]));
      door_frame2 += line_length;

      new_points.push_back (poly->points.at (inliers_right[i]));
      new_points.push_back (poly->points.at (inliers_right[i+1]));
    }
  }

  if (door_frame1 < min_height || door_frame2 < min_height)
    return (false);
  //new_points.push_back (poly->points[inliers_left[0]]);
  //poly->points.push_back (centroid);
//  poly->points = new_points;
  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Get the 3D bounds where the door will be searched for
  * \param p1 a point on the floor describing the door frame
  * \param p2 a point on the floor describing the door frame
  * \param min_b the minimum bounds (x-y-z)
  * \param max_b the maximum bounds (x-y-z)
  * \param min_z_bounds restrict the minimum search bounds on Z to this value
  * \param max_z_bounds restrict the maximum search bounds on Z to this value
  * \param multiplier multiply the ||p1-p2|| distance by this number to wrap all possible situations in X-Y
  */
void
  get3DBounds (Point32 *p1, Point32 *p2, Point32 &min_b, Point32 &max_b,
               double min_z_bounds, double max_z_bounds, int multiplier)
{
  // Get the door_frame distance in the X-Y plane
  float door_frame = sqrt ( (p1->x - p2->x) * (p1->x - p2->x) + (p1->y - p2->y) * (p1->y - p2->y) );

  float center[2];
  center[0] = (p1->x + p2->x) / 2.0;
  center[1] = (p1->y + p2->y) / 2.0;

  // Obtain the bounds (doesn't matter which is min and which is max at this point)
  min_b.x = center[0] + (multiplier * door_frame) / 2.0;
  min_b.y = center[1] + (multiplier * door_frame) / 2.0;
  min_b.z = min_z_bounds;

  max_b.x = center[0] - (multiplier * door_frame) / 2.0;
  max_b.y = center[1] - (multiplier * door_frame) / 2.0;
  max_b.z = max_z_bounds;

  // Order min/max
  if (min_b.x > max_b.x)
  {
    float tmp = min_b.x;
    min_b.x = max_b.x;
    max_b.x = tmp;
  }
  if (min_b.y > max_b.y)
  {
    float tmp = min_b.y;
    min_b.y = max_b.y;
    max_b.y = tmp;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Get the view point from where the scans were taken in the incoming PointCloud message frame
  * \param cloud_frame the point cloud message TF frame
  * \param viewpoint_cloud the resultant view point in the incoming cloud frame
  * \param tf a pointer to a TransformListener object
  */
void
  getCloudViewPoint (string cloud_frame, PointStamped &viewpoint_cloud, tf::TransformListener *tf)
{
  // Figure out the viewpoint value in the point cloud frame
  PointStamped viewpoint_laser;
  viewpoint_laser.header.frame_id = "laser_tilt_mount_link";
  // Set the viewpoint in the laser coordinate system to 0, 0, 0
  viewpoint_laser.point.x = viewpoint_laser.point.y = viewpoint_laser.point.z = 0.0;

  try
  {
    tf->transformPoint (cloud_frame, viewpoint_laser, viewpoint_cloud);
    ROS_INFO ("Cloud view point in frame %s is: %g, %g, %g.", cloud_frame.c_str (),
              viewpoint_cloud.point.x, viewpoint_cloud.point.y, viewpoint_cloud.point.z);
  }
  catch (tf::ConnectivityException)
  {
    ROS_WARN ("Could not transform a point from frame %s to frame %s!", viewpoint_laser.header.frame_id.c_str (), cloud_frame.c_str ());
    // Default to 0.05, 0, 0.942768
    viewpoint_cloud.point.x = 0.05; viewpoint_cloud.point.y = 0.0; viewpoint_cloud.point.z = 0.942768;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Get the best distribution statistics (mean and standard deviation) and select the inliers based on them
  * in a given channel space (specified by the d_idx index)
  * \param points a pointer to the point cloud message
  * \param indices a pointer to a set of point cloud indices to test
  * \param d_idx the dimension/channel index to test
  * \param inliers the resultant inliers
  */
void
  selectBestDistributionStatistics (PointCloud *points, vector<int> *indices, int d_idx, vector<int> &inliers)
{
  double mean, stddev;
  // Compute the mean and standard deviation of the distribution
  cloud_geometry::statistics::getChannelMeanStd (points, indices, d_idx, mean, stddev);
  //ROS_INFO ("Mean and standard deviation of the %s channel (%d) distribution: %g / %g.", points->chan[d_idx].name.c_str (), d_idx, mean, stddev);

  // (Chebyshev's inequality: at least 98% of the values are within 7 standard deviations from the mean)
  vector<int> alpha_vals (71);
  int nr_a = 0;
  for (double alpha = 0; alpha < 7; alpha += .1)
  {
    cloud_geometry::statistics::selectPointsOutsideDistribution (points, indices, d_idx, mean, stddev, alpha,
                                                     inliers);
    alpha_vals[nr_a] = inliers.size ();
    //ROS_INFO ("Number of points for %g: %d.", alpha, alpha_vals[nr_a]);
    nr_a++;
  }
  alpha_vals.resize (nr_a);

  // Compute the trimean of the distribution
  double trimean;
  cloud_geometry::statistics::getTrimean (alpha_vals, trimean);
  //ROS_INFO ("Trimean of the indices distribution: %g.", trimean);

  // Iterate over the list of alpha values to find the best one
  int best_i = 0;
  double best_alpha = DBL_MAX;
  for (unsigned int i = 0; i < alpha_vals.size (); i++)
  {
    double c_val = fabs ((double)alpha_vals[i] - trimean);
    if (c_val < best_alpha)       // Whenever we hit the same value, exit
    {
      best_alpha = c_val;
      best_i     = i;
    }
  }

  best_alpha = best_i / 10.0;
  //ROS_INFO ("Best alpha selected: %d / %d / %g", best_i, alpha_vals[best_i], best_alpha);

  // Select the inliers of the channel based on the best_alpha value
  cloud_geometry::statistics::selectPointsOutsideDistribution (points, indices, d_idx, mean, stddev, best_alpha, inliers);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Test whether the given point indices are roughly sampled at the viewpoint <-> plane perpendicular
  * \param points a pointer to the point cloud message
  * \param indices a pointer to a set of point cloud indices to test
  * \param viewpoint the viewpoint of the laser when the cloud was acquired
  * \param coeff the plane coefficients
  * \param eps_angle a maximum allowed angular threshold
  */
bool
  checkIfClusterPerpendicular (PointCloud *points, vector<int> *indices, PointStamped *viewpoint,
                               vector<double> *coeff, double eps_angle)
{
  // Compute the centroid of the cluster
  Point32 centroid;
  cloud_geometry::nearest::computeCentroid (points, indices, centroid);

  // Create a line direction from the viewpoint to the centroid
  centroid.x -= viewpoint->point.x;
  centroid.y -= viewpoint->point.y;
  centroid.z -= viewpoint->point.z;

  // Compute the normal of this cluster
  Eigen::Vector4d plane_parameters;
  double curvature;
  cloud_geometry::nearest::computeSurfaceNormalCurvature (points, indices, plane_parameters, curvature);
  Point32 normal;
  normal.x = plane_parameters (0);
  normal.y = plane_parameters (1);
  normal.z = plane_parameters (2);

//  vector<double> z_axis (3, 0); z_axis[2] = 1.0;
  // Compute the angle between the Z-axis and the newly created line direction
//  double angle = acos (cloud_geometry::dot (&centroid, &z_axis));//coeff));
//  if (fabs (M_PI / 2.0 - angle) < eps_angle)
  double angle = cloud_geometry::angles::getAngle3D (&centroid, &normal);
  if ( (angle < eps_angle) || ( (M_PI - angle) < eps_angle ) )
    return (true);
  return (false);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Decompose a region of space into clusters based on the euclidean distance between points, and the normal
  * angular deviation
  * \param points pointer to the point cloud message
  * \param indices pointer to a list of point indices
  * \param tolerance the spatial tolerance as a measure in the L2 Euclidean space
  * \param clusters the resultant clusters
  * \param nx_idx the index of the channel containing the X component of the normal
  * \param ny_idx the index of the channel containing the Y component of the normal
  * \param nz_idx the index of the channel containing the Z component of the normal
  * \param eps_angle the maximum allowed difference between normals in degrees for cluster/region growing
  * \param min_pts_per_cluster minimum number of points that a cluster may contain (default = 1)
  */
void
  findClusters (PointCloud *points, vector<int> *indices, double tolerance, vector<vector<int> > &clusters,
                int nx_idx, int ny_idx, int nz_idx,
                double eps_angle, unsigned int min_pts_per_cluster)
{
  // Create a tree for these points
  cloud_kdtree::KdTree* tree = new cloud_kdtree::KdTree (points, indices);

  // Create a bool vector of processed point indices, and initialize it to false
  vector<bool> processed;
  processed.resize (indices->size (), false);

  vector<int> nn_indices;
  // Process all points in the indices vector
  for (unsigned int i = 0; i < indices->size (); i++)
  {
    if (processed[i])
      continue;

    vector<int> seed_queue;
    int sq_idx = 0;
    seed_queue.push_back (i);

    double norm_a = 0.0;
    if (nx_idx != -1)         // If we use normal indices...
      norm_a = sqrt (points->chan[nx_idx].vals[indices->at (i)] * points->chan[nx_idx].vals[indices->at (i)] +
                     points->chan[ny_idx].vals[indices->at (i)] * points->chan[ny_idx].vals[indices->at (i)] +
                     points->chan[nz_idx].vals[indices->at (i)] * points->chan[nz_idx].vals[indices->at (i)]);

    processed[i] = true;

    while (sq_idx < (int)seed_queue.size ())
    {
      tree->radiusSearch (seed_queue.at (sq_idx), tolerance);
      tree->getNeighborsIndices (nn_indices);

      for (unsigned int j = 1; j < nn_indices.size (); j++)
      {
        if (!processed.at (nn_indices[j]))
        {
          if (nx_idx != -1)         // If we use normal indices...
          {
            double norm_b = sqrt (points->chan[nx_idx].vals[indices->at (nn_indices[j])] * points->chan[nx_idx].vals[indices->at (nn_indices[j])] +
                                  points->chan[ny_idx].vals[indices->at (nn_indices[j])] * points->chan[ny_idx].vals[indices->at (nn_indices[j])] +
                                  points->chan[nz_idx].vals[indices->at (nn_indices[j])] * points->chan[nz_idx].vals[indices->at (nn_indices[j])]);
            // [-1;1]
            double dot_p = points->chan[nx_idx].vals[indices->at (i)] * points->chan[nx_idx].vals[indices->at (nn_indices[j])] +
                           points->chan[ny_idx].vals[indices->at (i)] * points->chan[ny_idx].vals[indices->at (nn_indices[j])] +
                           points->chan[nz_idx].vals[indices->at (i)] * points->chan[nz_idx].vals[indices->at (nn_indices[j])];
            if ( acos (dot_p / (norm_a * norm_b)) < eps_angle)
            {
              processed[nn_indices[j]] = true;
              seed_queue.push_back (nn_indices[j]);
            }
          }
          else
          {
            processed[nn_indices[j]] = true;
            seed_queue.push_back (nn_indices[j]);
          }
        }
      }

      sq_idx++;
    }

    // If this queue is satisfactory, add to the clusters
    if (seed_queue.size () >= min_pts_per_cluster)
    {
      vector<int> r;
      //r.indices = seed_queue;
      r.resize (seed_queue.size ());
      for (unsigned int j = 0; j < r.size (); j++)
        r[j] = indices->at (seed_queue[j]);
      clusters.push_back (r);
    }
  }

  // Destroy the tree
  delete tree;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Find a plane model in a point cloud given via a set of point indices with SAmple Consensus methods
  * \param points the point cloud message
  * \param indices a pointer to a set of point cloud indices to test
  * \param inliers the resultant planar inliers
  * \param coeff the resultant plane coefficients
  * \param dist_thresh the maximum allowed distance threshold of an inlier to the model
  * \param min_pts the minimum number of points allowed as inliers for a plane model
  */
int
  fitSACPlane (PointCloud &points, vector<int> indices, vector<int> &inliers, vector<double> &coeff,
               robot_msgs::PointStamped *viewpoint_cloud, double dist_thresh, int min_pts)
{
  if ((int)indices.size () < min_pts)
  {
    inliers.resize (0);
    coeff.resize (0);
    return (-1);
  }

  // Create and initialize the SAC model
  sample_consensus::SACModelPlane *model = new sample_consensus::SACModelPlane ();
  //sample_consensus::SAC *sac             = new sample_consensus::RANSAC (model, dist_thresh);
  sample_consensus::SAC *sac             = new sample_consensus::LMedS (model, dist_thresh);
  sac->setMaxIterations (500);
  model->setDataSet (&points, indices);

  // Search for the best plane
  if (sac->computeModel ())
  {
    // Obtain the inliers and the planar model coefficients
    if ((int)sac->getInliers ().size () < min_pts)
    {
      //ROS_ERROR ("fitSACPlane: Inliers.size (%d) < sac_min_points_per_model (%d)!", sac->getInliers ().size (), sac_min_points_per_model_);
      inliers.resize (0);
      coeff.resize (0);
      return (-1);
    }
    inliers = sac->getInliers ();
    coeff   = sac->computeCoefficients ();

    // Flip plane normal according to the viewpoint information
    Point32 vp_m;
    vp_m.x = viewpoint_cloud->point.x - points.pts.at (inliers[0]).x;
    vp_m.y = viewpoint_cloud->point.y - points.pts.at (inliers[0]).y;
    vp_m.z = viewpoint_cloud->point.z - points.pts.at (inliers[0]).z;

    // Dot product between the (viewpoint - point) and the plane normal
    double cos_theta = (vp_m.x * coeff[0] + vp_m.y * coeff[1] + vp_m.z * coeff[2]);

    // Flip the plane normal
    if (cos_theta < 0)
    {
      for (int d = 0; d < 3; d++)
        coeff[d] *= -1;
      // Hessian form (D = nc . p_plane (centroid here) + p)
      coeff[3] = -1 * (coeff[0] * points.pts.at (inliers[0]).x + coeff[1] * points.pts.at (inliers[0]).y + coeff[2] * points.pts.at (inliers[0]).z);
    }
    //ROS_DEBUG ("Found a model supported by %d inliers: [%g, %g, %g, %g]\n", sac->getInliers ().size (),
    //           coeff[0], coeff[1], coeff[2], coeff[3]);

    // Project the inliers onto the model
    model->projectPointsInPlace (inliers, coeff);
  }
  else
  {
    ROS_ERROR ("Could not compute a plane model.");
    return (-1);
  }
  delete sac;
  delete model;
  return (0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Estimate point normals for a given point cloud message (in place)
  * \param points the original point cloud message
  * \param points_down the downsampled point cloud message
  * \param k the number of nearest neighbors to search for
  * \param viewpoint_cloud a pointer to the viewpoint where the cloud was acquired from (used for normal flip)
  */
void
  estimatePointNormals (PointCloud *points, vector<int> *point_indices, PointCloud *points_down, int k, PointStamped *viewpoint_cloud)
{
  // Reserve space for 4 channels: nx, ny, nz, curvature
  points_down->chan.resize (4);
  points_down->chan[0].name = "nx";
  points_down->chan[1].name = "ny";
  points_down->chan[2].name = "nz";
  points_down->chan[3].name = "curvature";
  for (unsigned int d = 0; d < points_down->chan.size (); d++)
    points_down->chan[d].vals.resize (points_down->pts.size ());

  cloud_kdtree::KdTree *kdtree = new cloud_kdtree::KdTree (points);

  // Allocate enough space for point indices
  vector<vector<int> > points_k_indices (points_down->pts.size ());
  vector<double> distances (points->pts.size ());

  // Get the nearest neighbors for all the point indices in the bounds
  for (int i = 0; i < (int)points_down->pts.size (); i++)
  {
    //kdtree->nearestKSearch (i, k, points_k_indices[i], distances);
    kdtree->radiusSearch (&points_down->pts[i], 0.035, points_k_indices[i], distances);
  }

#pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < (int)points_down->pts.size (); i++)
  {
    // Compute the point normals (nx, ny, nz), surface curvature estimates (c)
    Eigen::Vector4d plane_parameters;
    double curvature;
    cloud_geometry::nearest::computeSurfaceNormalCurvature (points, &points_k_indices[i], plane_parameters, curvature);

    // See if we need to flip any plane normals
    Point32 vp_m;
    vp_m.x = viewpoint_cloud->point.x - points_down->pts[i].x;
    vp_m.y = viewpoint_cloud->point.y - points_down->pts[i].y;
    vp_m.z = viewpoint_cloud->point.z - points_down->pts[i].z;

    // Dot product between the (viewpoint - point) and the plane normal
    double cos_theta = (vp_m.x * plane_parameters (0) + vp_m.y * plane_parameters (1) + vp_m.z * plane_parameters (2));

    // Flip the plane normal
    if (cos_theta < 0)
    {
      for (int d = 0; d < 3; d++)
        plane_parameters (d) *= -1;
    }
    points_down->chan[0].vals[i] = plane_parameters (0);
    points_down->chan[1].vals[i] = plane_parameters (1);
    points_down->chan[2].vals[i] = plane_parameters (2);
    points_down->chan[3].vals[i] = fabs (plane_parameters (3));
  }
  // Delete the kd-tree
  delete kdtree;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Finds the best oriented line in points/indices with respect to a given axis and return the point inliers.
  * \param points a pointer to the point cloud message
  * \param indices the point cloud indices to use
  * \param dist_thresh maximum allowed distance threshold of an inlier point to the line model
  * \param axis the axis to check against
  * \param eps_angle maximum angular line deviation from the axis (in radians)
  * \param line_inliers the resultant point inliers
  */
int
  fitSACOrientedLine (robot_msgs::PointCloud *points, std::vector<int> indices,
                      double dist_thresh, robot_msgs::Point32 *axis, double eps_angle, std::vector<int> &line_inliers)
{
  if (indices.size () < 5)
  {
    line_inliers.resize (0);
    return (-1);
  }

  // Create and initialize the SAC model
  sample_consensus::SACModelOrientedLine *model = new sample_consensus::SACModelOrientedLine ();
  sample_consensus::SAC *sac             = new sample_consensus::RANSAC (model, dist_thresh);
  sac->setMaxIterations (100);
  model->setDataSet (points, indices);
  model->setAxis (axis);
  model->setEpsAngle (eps_angle);

  // Search for the best line
  if (sac->computeModel ())
  {
    line_inliers = sac->getInliers ();
  }
  else
  {
    ROS_ERROR ("Could not compute an oriented line model.");
    return (-1);
  }
  delete sac;
  delete model;
  return (0);
}

