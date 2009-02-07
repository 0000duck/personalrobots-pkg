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

#ifndef _CLOUD_GEOMETRY_POINT_H_
#define _CLOUD_GEOMETRY_POINT_H_

// ROS includes
#include <std_msgs/PointCloud.h>
#include <std_msgs/Polygon3D.h>
#include <std_msgs/Point32.h>
#include <std_msgs/Point.h>
#include <math.h>

#include <Eigen/Core>

#include <cfloat>

namespace cloud_geometry
{

  /** \brief Simple leaf (3d box) structure) holding a centroid and the number of points in the leaf */
  struct Leaf
  {
    float centroid_x, centroid_y, centroid_z;
    unsigned short nr_points;
  };


  int getChannelIndex (std_msgs::PointCloud *points, std::string channel_name);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Create a quick copy of a point and its associated channels and return the data as a float vector.
    * \param points the point cloud data message
    * \param index the index of the point to return
    * \param array the vector containing the result
    */
  inline void
    getPointAsFloatArray (std_msgs::PointCloud points, int index, std::vector<float> &array)
  {
    // Resize for XYZ (3) + NR_CHANNELS
    array.resize (3 + points.get_chan_size ());
    array[0] = points.pts[index].x;
    array[1] = points.pts[index].y;
    array[2] = points.pts[index].z;

    for (unsigned int d = 0; d < points.get_chan_size (); d++)
      array[3 + d] = points.chan[d].vals[index];
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Create a quick copy of a point and its associated channels and return the data as a float vector.
    * \param points the point cloud data message
    * \param index the index of the point to return
    * \param array the vector containing the result
    * \param nr_channels the number of channels to copy (starting with the first channel)
    */
  inline void
    getPointAsFloatArray (std_msgs::PointCloud points, int index, std::vector<float> &array, int nr_channels)
  {
    // Resize for XYZ (3) + NR_CHANNELS
    array.resize (3 + nr_channels);
    array[0] = points.pts[index].x;
    array[1] = points.pts[index].y;
    array[2] = points.pts[index].z;

    for (int d = 0; d < nr_channels; d++)
      array[3 + d] = points.chan[d].vals[index];
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Create a quick copy of a point and its associated channels and return the data as a float vector.
    * \param points the point cloud data message
    * \param index the index of the point to return
    * \param array the vector containing the result
    * \param start_channel the first channel to start copying data from
    * \param end_channel the last channel to stop copying data at
    */
  inline void
    getPointAsFloatArray (std_msgs::PointCloud points, int index, std::vector<float> &array, int start_channel, int end_channel)
  {
    // Resize for XYZ (3) + NR_CHANNELS
    array.resize (3 + end_channel - start_channel);
    array[0] = points.pts[index].x;
    array[1] = points.pts[index].y;
    array[2] = points.pts[index].z;

    for (int d = start_channel; d < end_channel; d++)
      array[3 + d - start_channel] = points.chan[d].vals[index];
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Create a quick copy of a point and its associated channels and return the data as a float vector.
    * \param points the point cloud data message
    * \param index the index of the point to return
    * \param array the vector containing the result
    * \param start_channel the first channel to start copying data from
    * \param end_channel the last channel to stop copying data at
    */
  inline void
    getPointAsFloatArray (std_msgs::PointCloud points, int index, std::vector<float> &array, std::vector<int> channels)
  {
    if (channels.size () > points.get_chan_size ())
      return;
    // Resize for XYZ (3) + NR_CHANNELS
    array.resize (3 + channels.size ());
    array[0] = points.pts[index].x;
    array[1] = points.pts[index].y;
    array[2] = points.pts[index].z;

    for (unsigned int d = 0; d < channels.size (); d++)
      array[3 + d] = points.chan[channels.at (d)].vals[index];
  }

  std_msgs::Point32 computeMedian (std_msgs::PointCloud points);
  std_msgs::Point32 computeMedian (std_msgs::PointCloud points, std::vector<int> indices);

  double computeMedianAbsoluteDeviation (std_msgs::PointCloud points, double sigma);
  double computeMedianAbsoluteDeviation (std_msgs::PointCloud points, std::vector<int> indices, double sigma);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Compute the centralized moment at a 3D points patch
    * \param points the point cloud data message
    * \param p the p-dimension
    * \param q the q-dimension
    * \param r the r-dimension
    */
  inline double
    computeCentralizedMoment (std_msgs::PointCloud *points, double p, double q, double r)
  {
    double result = 0.0;
    for (unsigned int cp = 0; cp < points->pts.size (); cp++)
      result += pow (points->pts[cp].x, p) * pow (points->pts[cp].y, q) * pow (points->pts[cp].z, r);

    return (result);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Compute the centralized moment at a 3D points patch, using their indices.
    * \param points the point cloud data message
    * \param indices the point cloud indices that need to be used
    * \param p the p-dimension
    * \param q the q-dimension
    * \param r the r-dimension
    */
  inline double
    computeCentralizedMoment (std_msgs::PointCloud *points, std::vector<int> *indices, double p, double q, double r)
  {
    double result = 0.0;
    for (unsigned int cp = 0; cp < indices->size (); cp++)
      result += pow (points->pts.at (indices->at (cp)).x, p) * pow (points->pts.at (indices->at (cp)).y, q) * pow (points->pts.at (indices->at (cp)).z, r);

    return (result);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Compute the cross product between two points (vectors).
    * \param p1 the first point/vector
    * \param p2 the second point/vector
    */
  inline std_msgs::Point32
    cross (std_msgs::Point32 p1, std_msgs::Point32 p2)
  {
    std_msgs::Point32 r;
    r.x = p1.y*p2.z - p1.z*p2.y;
    r.y = p1.z*p2.x - p1.x*p2.z;
    r.z = p1.x*p2.y - p1.y*p2.x;
    return (r);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Compute the dot product between two points (vectors).
    * \param p1 the first point/vector
    * \param p2 the second point/vector
    */
  inline double
    dot (std_msgs::Point32 p1, std_msgs::Point32 p2)
  {
    return (p1.x * p2.x + p1.y * p2.y + p1.z * p2.z);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Normalize a point.
    * \param p the point/vector to normalize
    * \param q the resulted normalized point/vector
    */
  inline void
    normalizePoint (std_msgs::Point32 p, std_msgs::Point32 &q)
  {
    // Calculate the 2-norm: norm (x) = sqrt (sum (abs (v)^2))
    double n_norm = sqrt (p.x * p.x + p.y * p.y + p.z * p.z);
    q.x = p.x / n_norm;
    q.y = p.y / n_norm;
    q.z = p.z / n_norm;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Normalize a point and return the result in place.
    * \param p the point/vector to normalize
    */
  inline void
    normalizePoint (std_msgs::Point32 &p)
  {
    normalizePoint (p, p);
  }

  void getPointIndicesAxisParallelNormals (std_msgs::PointCloud *points, int nx, int ny, int nz, double eps_angle,
                                           std_msgs::Point32 axis, std::vector<int> &indices);
  void getPointIndicesAxisPerpendicularNormals (std_msgs::PointCloud *points, int nx, int ny, int nz, double eps_angle,
                                                std_msgs::Point32 axis, std::vector<int> &indices);


  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Determine the minimum and maximum 3D bounding box coordinates for a given point cloud
    * \param points the point cloud message
    * \param minP the resultant minimum bounding box coordinates
    * \param maxP the resultant maximum bounding box coordinates
    */
  inline void
    getMinMax (std_msgs::PointCloud *points, std_msgs::Point32 &minP, std_msgs::Point32 &maxP)
  {
    minP.x = minP.y = minP.z = FLT_MAX;
    maxP.x = maxP.y = maxP.z = FLT_MIN;

    for (unsigned int i = 0; i < points->pts.size (); i++)
    {
      minP.x = (points->pts[i].x < minP.x) ? points->pts[i].x : minP.x;
      minP.y = (points->pts[i].y < minP.y) ? points->pts[i].y : minP.y;
      minP.z = (points->pts[i].z < minP.z) ? points->pts[i].z : minP.z;

      maxP.x = (points->pts[i].x > maxP.x) ? points->pts[i].x : maxP.x;
      maxP.y = (points->pts[i].y > maxP.y) ? points->pts[i].y : maxP.y;
      maxP.z = (points->pts[i].z > maxP.z) ? points->pts[i].z : maxP.z;
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Determine the minimum and maximum 3D bounding box coordinates for a given point cloud
    * \param points the point cloud message
    * \param minP the resultant minimum bounding box coordinates
    * \param maxP the resultant maximum bounding box coordinates
    */
  inline void
    getMinMax (std_msgs::Polygon3D *poly, std_msgs::Point32 &minP, std_msgs::Point32 &maxP)
  {
    minP.x = minP.y = minP.z = FLT_MAX;
    maxP.x = maxP.y = maxP.z = FLT_MIN;

    for (unsigned int i = 0; i < poly->points.size (); i++)
    {
      minP.x = (poly->points[i].x < minP.x) ? poly->points[i].x : minP.x;
      minP.y = (poly->points[i].y < minP.y) ? poly->points[i].y : minP.y;
      minP.z = (poly->points[i].z < minP.z) ? poly->points[i].z : minP.z;

      maxP.x = (poly->points[i].x > maxP.x) ? poly->points[i].x : maxP.x;
      maxP.y = (poly->points[i].y > maxP.y) ? poly->points[i].y : maxP.y;
      maxP.z = (poly->points[i].z > maxP.z) ? poly->points[i].z : maxP.z;
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Determine the minimum and maximum 3D bounding box coordinates for a given point cloud, using point indices
    * \param points the point cloud message
    * \param indices the point cloud indices to use
    * \param minP the resultant minimum bounding box coordinates
    * \param maxP the resultant maximum bounding box coordinates
    */
  inline void
    getMinMax (std_msgs::PointCloud *points, std::vector<int> *indices, std_msgs::Point32 &minP, std_msgs::Point32 &maxP)
  {
    minP.x = minP.y = minP.z = FLT_MAX;
    maxP.x = maxP.y = maxP.z = FLT_MIN;

    for (unsigned int i = 0; i < indices->size (); i++)
    {
      minP.x = (points->pts.at (indices->at (i)).x < minP.x) ? points->pts.at (indices->at (i)).x : minP.x;
      minP.y = (points->pts.at (indices->at (i)).y < minP.y) ? points->pts.at (indices->at (i)).y : minP.y;
      minP.z = (points->pts.at (indices->at (i)).z < minP.z) ? points->pts.at (indices->at (i)).z : minP.z;

      maxP.x = (points->pts.at (indices->at (i)).x > maxP.x) ? points->pts.at (indices->at (i)).x : maxP.x;
      maxP.y = (points->pts.at (indices->at (i)).y > maxP.y) ? points->pts.at (indices->at (i)).y : maxP.y;
      maxP.z = (points->pts.at (indices->at (i)).z > maxP.z) ? points->pts.at (indices->at (i)).z : maxP.z;
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Get the minimum and maximum values on each of the 3 (x-y-z) dimensions
    * in a given pointcloud, without considering points outside of a distance threshold from the laser origin
    * \param points the point cloud data message
    * \param min_pt the resultant minimum bounds
    * \param max_pt the resultant maximum bounds
    * \param c_idx the index of the channel holding distance information
    * \param cut_distance a maximum admissible distance threshold for points from the laser origin
    */
  inline void
    getMinMax (std_msgs::PointCloud *points, std_msgs::Point32 &min_pt, std_msgs::Point32 &max_pt,
               int c_idx, double cut_distance)
  {
    min_pt.x = min_pt.y = min_pt.z = FLT_MAX;
    max_pt.x = max_pt.y = max_pt.z = FLT_MIN;

    for (unsigned int i = 0; i < points->pts.size (); i++)
    {
      if (c_idx != -1 && points->chan[c_idx].vals[i] > cut_distance)
        continue;
      min_pt.x = (points->pts[i].x < min_pt.x) ? points->pts[i].x : min_pt.x;
      min_pt.y = (points->pts[i].y < min_pt.y) ? points->pts[i].y : min_pt.y;
      min_pt.z = (points->pts[i].z < min_pt.z) ? points->pts[i].z : min_pt.z;

      max_pt.x = (points->pts[i].x > max_pt.x) ? points->pts[i].x : max_pt.x;
      max_pt.y = (points->pts[i].y > max_pt.y) ? points->pts[i].y : max_pt.y;
      max_pt.z = (points->pts[i].z > max_pt.z) ? points->pts[i].z : max_pt.z;
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Get the minimum and maximum values on each of the 3 (x-y-z) dimensions
    * in a given pointcloud, without considering points outside of a distance threshold from the laser origin
    * \param points the point cloud data message
    * \param indices the point cloud indices to use
    * \param min_pt the resultant minimum bounds
    * \param max_pt the resultant maximum bounds
    * \param c_idx the index of the channel holding distance information
    * \param cut_distance a maximum admissible distance threshold for points from the laser origin
    */
  inline void
    getMinMax (std_msgs::PointCloud *points, std::vector<int> *indices, std_msgs::Point32 &min_pt, std_msgs::Point32 &max_pt,
               int c_idx, double cut_distance)
  {
    min_pt.x = min_pt.y = min_pt.z = FLT_MAX;
    max_pt.x = max_pt.y = max_pt.z = FLT_MIN;

    for (unsigned int i = 0; i < indices->size (); i++)
    {
      if (c_idx != -1 && points->chan[c_idx].vals[indices->at (i)] > cut_distance)
        continue;
      min_pt.x = (points->pts[indices->at (i)].x < min_pt.x) ? points->pts[indices->at (i)].x : min_pt.x;
      min_pt.y = (points->pts[indices->at (i)].y < min_pt.y) ? points->pts[indices->at (i)].y : min_pt.y;
      min_pt.z = (points->pts[indices->at (i)].z < min_pt.z) ? points->pts[indices->at (i)].z : min_pt.z;

      max_pt.x = (points->pts[indices->at (i)].x > max_pt.x) ? points->pts[indices->at (i)].x : max_pt.x;
      max_pt.y = (points->pts[indices->at (i)].y > max_pt.y) ? points->pts[indices->at (i)].y : max_pt.y;
      max_pt.z = (points->pts[indices->at (i)].z > max_pt.z) ? points->pts[indices->at (i)].z : max_pt.z;
    }
  }

  void downsamplePointCloud (std_msgs::PointCloud *points, std::vector<int> *indices, std_msgs::PointCloud &points_down, std_msgs::Point leaf_size,
                             std::vector<Leaf> &leaves, int d_idx, double cut_distance = DBL_MAX);

  void downsamplePointCloud (std_msgs::PointCloud *points, std_msgs::PointCloud &points_down, std_msgs::Point leaf_size,
                             std::vector<Leaf> &leaves, int d_idx, double cut_distance = DBL_MAX);

  void downsamplePointCloud (std_msgs::PointCloud *points, std_msgs::PointCloud &points_down, std_msgs::Point leaf_size);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Compute the angle in the [ 0, 2*PI ) interval of a point (direction) with a reference (0, 0) in 2D.
    * \param point a 2D point
    */
  inline double
    getAngle2D (double point[2])
  {
    double rad;
    if (point[0] == 0)
      rad = (point[1] < 0) ? -M_PI / 2.0 : M_PI / 2.0;
    else
    {
      rad = atan (point[1] / point[0]);
      if (point[0] < 0)
        rad += M_PI;
    }
    if (rad < 0)
      rad += 2 * M_PI;

    return (rad);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Get a u-v-n coordinate system that lies on a plane defined by its normal
    * \param plane_coeff the plane coefficients (containing n, the plane normal)
    * \param u the resultant u direction
    * \param v the resultant v direction
    */
  inline void
    getCoordinateSystemOnPlane (std::vector<double> *plane_coeff, Eigen::Vector3d &u, Eigen::Vector3d &v)
  {
    // Initialize normalized u vector with "random" values not parallel with normal
    u (1) = 0;
    if (fabs (plane_coeff->at (0)) != 1)
    {
      u (0) = 1;
      u (2) = 0;
    }
    else
    {
      u (0) = 0;
      u (2) = 1;
    }

    // Compute the v vector and normalize it
    v (0) = plane_coeff->at (1) * u (2) - plane_coeff->at (2) * u (1);
    v (1) = plane_coeff->at (2) * u (0) - plane_coeff->at (0) * u (2);
    v (2) = plane_coeff->at (0) * u (1) - plane_coeff->at (1) * u (0);
    double v_length = sqrt (v (0) * v (0) + v (1) * v (1) + v (2) * v (2));
    v (0) /= v_length;
    v (1) /= v_length;
    v (2) /= v_length;

    // Recompute u and normalize it
    u (0) = v (1) * plane_coeff->at (2) - v (2) * plane_coeff->at (1);
    u (1) = v (2) * plane_coeff->at (0) - v (0) * plane_coeff->at (2);
    u (2) = v (0) * plane_coeff->at (1) - v (1) * plane_coeff->at (0);
    double u_length = sqrt (u (0) * u (0) + u (1) * u (1) + u (2) * u (2));
    u (0) /= u_length;
    u (1) /= u_length;
    u (2) /= u_length;
  }


  void getChannelMeanStd (std_msgs::PointCloud *points, int d_idx, double &mean, double &stddev);
  void getChannelMeanStd (std_msgs::PointCloud *points, std::vector<int> *indices, int d_idx, double &mean, double &stddev);

  void selectPointsOutsideDistribution (std_msgs::PointCloud *points, std::vector<int> *indices, int d_idx,
                                        double mean, double stddev, double alpha, std::vector<int> &inliers);
  void selectPointsInsideDistribution (std_msgs::PointCloud *points, std::vector<int> *indices, int d_idx,
                                       double mean, double stddev, double alpha, std::vector<int> &inliers);


  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /** \brief Write the point data to screen (stderr)
    * \param p the point
    */
  inline void
    cerr_p (std_msgs::Point32 p)
  {
    std::cerr << p.x << " " << p.y << " " << p.z << std::endl;
  }

}

#endif
