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

#include <cloud_geometry/angles.h>
#include <cloud_geometry/nearest.h>
#include <cloud_geometry/point.h>
#include <cloud_geometry/statistics.h>

namespace cloud_geometry
{

  namespace nearest
  {

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the centroid of a set of points and return it as a PointCloud message with 1 value.
      * \param points the input point cloud
      * \param centroid the output centroid
      */
    void
      computeCentroid (robot_msgs::PointCloud *points, robot_msgs::PointCloud &centroid)
    {
      // Prepare the data output
      centroid.pts.resize (1);
      centroid.pts[0].x = centroid.pts[0].y = centroid.pts[0].z = 0;
      centroid.chan.resize (points->get_chan_size ());
      for (unsigned int d = 0; d < points->get_chan_size (); d++)
      {
        centroid.chan[d].name = points->chan[d].name;
        centroid.chan[d].vals.resize (1);
      }

      // For each point in the cloud
      for (unsigned int i = 0; i < points->get_pts_size (); i++)
      {
        centroid.pts[0].x += points->pts[i].x;
        centroid.pts[0].y += points->pts[i].y;
        centroid.pts[0].z += points->pts[i].z;

        for (unsigned int d = 0; d < points->get_chan_size (); d++)
          centroid.chan[d].vals[0] += points->chan[d].vals[i];
      }

      centroid.pts[0].x /= points->get_pts_size ();
      centroid.pts[0].y /= points->get_pts_size ();
      centroid.pts[0].z /= points->get_pts_size ();
      for (unsigned int d = 0; d < points->get_chan_size (); d++)
        centroid.chan[d].vals[0] /= points->get_pts_size ();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the centroid of a set of points using their indices and return it as a PointCloud message with 1 value.
      * \param points the input point cloud
      * \param indices the point cloud indices that need to be used
      * \param centroid the output centroid
      */
    void
      computeCentroid (robot_msgs::PointCloud *points, std::vector<int> *indices, robot_msgs::PointCloud &centroid)
    {
      // Prepare the data output
      centroid.pts.resize (1);
      centroid.pts[0].x = centroid.pts[0].y = centroid.pts[0].z = 0;
      centroid.chan.resize (points->get_chan_size ());
      for (unsigned int d = 0; d < points->get_chan_size (); d++)
      {
        centroid.chan[d].name = points->chan[d].name;
        centroid.chan[d].vals.resize (1);
      }

      // For each point in the cloud
      for (unsigned int i = 0; i < indices->size (); i++)
      {
        centroid.pts[0].x += points->pts.at (indices->at (i)).x;
        centroid.pts[0].y += points->pts.at (indices->at (i)).y;
        centroid.pts[0].z += points->pts.at (indices->at (i)).z;

        for (unsigned int d = 0; d < points->get_chan_size (); d++)
          centroid.chan[d].vals[0] += points->chan[d].vals.at (indices->at (i));
      }

      centroid.pts[0].x /= indices->size ();
      centroid.pts[0].y /= indices->size ();
      centroid.pts[0].z /= indices->size ();
      for (unsigned int d = 0; d < points->get_chan_size (); d++)
        centroid.chan[d].vals[0] /= indices->size ();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the Least-Squares plane fit for a given set of points, and return the estimated plane parameters
      * together with the surface curvature.
      * \param points the input point cloud
      * \param plane_parameters the plane parameters as: a, b, c, d (ax + by + cz + d = 0)
      * \param curvature the estimated surface curvature as a measure of
      * \f[
      * \lambda_0 / (\lambda_0 + \lambda_1 + \lambda_2)
      * \f]
      */
    void
      computeSurfaceNormalCurvature (robot_msgs::PointCloud *points, Eigen::Vector4d &plane_parameters, double &curvature)
    {
      robot_msgs::Point32 centroid;
      // Compute the 3x3 covariance matrix
      Eigen::Matrix3d covariance_matrix;
      computeCovarianceMatrix (points, covariance_matrix, centroid);

      // Extract the eigenvalues and eigenvectors
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> ei_symm (covariance_matrix);
      Eigen::Vector3d eigen_values  = ei_symm.eigenvalues ();
      Eigen::Matrix3d eigen_vectors = ei_symm.eigenvectors ();
      //eigen_cov (covariance_matrix, eigen_values, eigen_vectors);

      // Normalize the surface normal (eigenvector corresponding to the smallest eigenvalue)
      double norm = sqrt ( eigen_vectors (0, 0) * eigen_vectors (0, 0) +
                           eigen_vectors (0, 1) * eigen_vectors (0, 1) +
                           eigen_vectors (0, 2) * eigen_vectors (0, 2));
      plane_parameters (0) = eigen_vectors (0, 0) / norm;
      plane_parameters (1) = eigen_vectors (0, 1) / norm;
      plane_parameters (2) = eigen_vectors (0, 2) / norm;

      // Hessian form (D = nc . p_plane (centroid here) + p)
      plane_parameters (3) = -1 * (plane_parameters (0) * centroid.x + plane_parameters (1) * centroid.y + plane_parameters (2) * centroid.z);

      // Compute the curvature surface change
      curvature = fabs ( eigen_values (0) / (eigen_values (0) + eigen_values (1) + eigen_values (2)) );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the Least-Squares plane fit for a given set of points, using their indices,
      * and return the estimated plane parameters together with the surface curvature.
      * \param points the input point cloud
      * \param indices the point cloud indices that need to be used
      * \param plane_parameters the plane parameters as: a, b, c, d (ax + by + cz + d = 0)
      * \param curvature the estimated surface curvature as a measure of
      * \f[
      * \lambda_0 / (\lambda_0 + \lambda_1 + \lambda_2)
      * \f]
      */
    void
      computeSurfaceNormalCurvature (robot_msgs::PointCloud *points, std::vector<int> *indices, Eigen::Vector4d &plane_parameters, double &curvature)
    {
      robot_msgs::Point32 centroid;
      // Compute the 3x3 covariance matrix
      Eigen::Matrix3d covariance_matrix;
      computeCovarianceMatrix (points, indices, covariance_matrix, centroid);

      // Extract the eigenvalues and eigenvectors
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> ei_symm (covariance_matrix);
      Eigen::Vector3d eigen_values  = ei_symm.eigenvalues ();
      Eigen::Matrix3d eigen_vectors = ei_symm.eigenvectors ();

      // Normalize the surface normal (eigenvector corresponding to the smallest eigenvalue)
      // Note: Remember to take care of the eigen_vectors ordering
      double norm = sqrt ( eigen_vectors (0, 0) * eigen_vectors (0, 0) +
                           eigen_vectors (1, 0) * eigen_vectors (1, 0) +
                           eigen_vectors (2, 0) * eigen_vectors (2, 0));
      plane_parameters (0) = eigen_vectors (0, 0) / norm;
      plane_parameters (1) = eigen_vectors (1, 0) / norm;
      plane_parameters (2) = eigen_vectors (2, 0) / norm;

      // Hessian form (D = nc . p_plane (centroid here) + p)
      plane_parameters (3) = -1 * (plane_parameters (0) * centroid.x + plane_parameters (1) * centroid.y + plane_parameters (2) * centroid.z);

      // Compute the curvature surface change
      curvature = fabs ( eigen_values (0) / (eigen_values (0) + eigen_values (1) + eigen_values (2)) );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the 3 moment invariants (j1, j2, j3) for a given set of points.
      * \param points the input point cloud
      * \param j1 the first moment invariant
      * \param j2 the second moment invariant
      * \param j3 the third moment invariant
      */
    void
      computeMomentInvariants (robot_msgs::PointCloud *points, double &j1, double &j2, double &j3)
    {
      // Compute the centroid
      robot_msgs::Point32 centroid;
      computeCentroid (points, centroid);

      // Demean the pointset
      robot_msgs::PointCloud points_c;
      points_c.pts.resize (points->pts.size ());
      for (unsigned int i = 0; i < points->pts.size (); i++)
      {
        points_c.pts[i].x = points->pts[i].x - centroid.x;
        points_c.pts[i].y = points->pts[i].y - centroid.y;
        points_c.pts[i].z = points->pts[i].z - centroid.z;
      }

      double mu200 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 2.0, 0.0, 0.0);
      double mu020 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 0.0, 2.0, 0.0);
      double mu002 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 0.0, 0.0, 2.0);
      double mu110 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 1.0, 1.0, 0.0);
      double mu101 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 1.0, 0.0, 1.0);
      double mu011 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 0.0, 1.0, 1.0);

      j1 = mu200 + mu020 + mu002;
      j2 = mu200*mu020 + mu200*mu002 + mu020*mu002 - mu110*mu110 - mu101*mu101 - mu011*mu011;
      j3 = mu200*mu020*mu002 + 2*mu110*mu101*mu011 - mu002*mu110*mu110 - mu020*mu101*mu101 - mu200*mu011*mu011;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the 3 moment invariants (j1, j2, j3) for a given set of points, using their indices.
      * \param points the input point cloud
      * \param indices the point cloud indices that need to be used
      * \param j1 the first moment invariant
      * \param j2 the second moment invariant
      * \param j3 the third moment invariant
      */
    void
      computeMomentInvariants (robot_msgs::PointCloud *points, std::vector<int> *indices, double &j1, double &j2, double &j3)
    {
      // Compute the centroid
      robot_msgs::Point32 centroid;
      computeCentroid (points, indices, centroid);

      // Demean the pointset
      robot_msgs::PointCloud points_c;
      points_c.pts.resize (indices->size ());
      for (unsigned int i = 0; i < indices->size (); i++)
      {
        points_c.pts[i].x = points->pts.at (indices->at (i)).x - centroid.x;
        points_c.pts[i].y = points->pts.at (indices->at (i)).y - centroid.y;
        points_c.pts[i].z = points->pts.at (indices->at (i)).z - centroid.z;
      }

      double mu200 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 2.0, 0.0, 0.0);
      double mu020 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 0.0, 2.0, 0.0);
      double mu002 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 0.0, 0.0, 2.0);
      double mu110 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 1.0, 1.0, 0.0);
      double mu101 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 1.0, 0.0, 1.0);
      double mu011 = cloud_geometry::statistics::computeCentralizedMoment (&points_c, 0.0, 1.0, 1.0);

      j1 = mu200 + mu020 + mu002;
      j2 = mu200*mu020 + mu200*mu002 + mu020*mu002 - mu110*mu110 - mu101*mu101 - mu011*mu011;
      j3 = mu200*mu020*mu002 + 2*mu110*mu101*mu011 - mu002*mu110*mu110 - mu020*mu101*mu101 - mu200*mu011*mu011;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Check whether a point is a boundary point in a planar patch of projected points given by indices.
      * \note A coordinate system u-v-n must be computed a-priori using \a getCoordinateSystemOnPlane
      * \param points a pointer to the input point cloud
      * \param q_idx the index of the query point in \a points
      * \param neighbors the estimated point neighbors of the query point
      * \param u the u direction
      * \param v the v direction
      * \param angle_threshold the threshold angle (default $\pi / 2.0$)
      */
    bool
      isBoundaryPoint (robot_msgs::PointCloud *points, int q_idx, std::vector<int> *neighbors,
                       Eigen::Vector3d u, Eigen::Vector3d v, double angle_threshold)
    {
      if (neighbors->size () < 3)
        return (false);
      double uvn_nn[2];
      // Compute the angles between each neighbouring point and the query point itself
      std::vector<double> angles (neighbors->size () - 1);
      for (unsigned int i = 0; i < neighbors->size () - 1; i++)
      {
        uvn_nn[0] = (points->pts.at (neighbors->at (i+1)).x - points->pts.at (q_idx).x) * u (0) +
                    (points->pts.at (neighbors->at (i+1)).y - points->pts.at (q_idx).y) * u (1) +
                    (points->pts.at (neighbors->at (i+1)).z - points->pts.at (q_idx).z) * u (2);
        uvn_nn[1] = (points->pts.at (neighbors->at (i+1)).x - points->pts.at (q_idx).x) * v (0) +
                    (points->pts.at (neighbors->at (i+1)).y - points->pts.at (q_idx).y) * v (1) +
                    (points->pts.at (neighbors->at (i+1)).z - points->pts.at (q_idx).z) * v (2);
        angles[i] = cloud_geometry::angles::getAngle2D (uvn_nn);
      }
      sort (angles.begin (), angles.end ());

      // Compute the maximal angle difference between two consecutive angles
      double max_dif = DBL_MIN, dif;
      for (unsigned int i = 0; i < neighbors->size () - 2; i++)
      {
        dif = angles[i + 1] - angles[i];
        if (max_dif < dif)
          max_dif = dif;
      }
      // Get the angle difference between the last and the first
      dif = 2 * M_PI - angles[neighbors->size () - 2] + angles[0];
      if (max_dif < dif)
        max_dif = dif;

      // Check results
      if (max_dif > angle_threshold)
        return (true);
      else
        return (false);
    }

  }
}
