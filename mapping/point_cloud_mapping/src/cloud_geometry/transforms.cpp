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
#include <cloud_geometry/transforms.h>

namespace cloud_geometry
{
  namespace transforms
  {

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Obtain a 4x4 rigid transformation matrix (with translation)
      * \param plane_a the normalized coefficients of the first plane
      * \param plane_b the normalized coefficients of the second plane
      * \param tx the desired translation on x-axis
      * \param ty the desired translation on y-axis
      * \param tz the desired translation on z-axis
      * \param transformation the resultant transformation matrix
      */
    void
      getPlaneToPlaneTransformation (std::vector<double> plane_a, std::vector<double> plane_b,
                                     float tx, float ty, float tz, Eigen::Matrix4d &transformation)
    {
      double angle = cloud_geometry::angles::getAngleBetweenPlanes (plane_a, plane_b);
      // Compute the rotation axis R = Nplane x (0, 0, 1)
      robot_msgs::Point32 r_axis;
      r_axis.x = plane_a[1]*plane_b[2] - plane_a[2]*plane_b[1];
      r_axis.y = plane_a[2]*plane_b[0] - plane_a[0]*plane_b[2];
      r_axis.z = plane_a[0]*plane_b[1] - plane_a[1]*plane_b[0];

      if (r_axis.z < 0)
        angle = -angle;

      // Build a normalized quaternion
      double s = sin (0.5 * angle) / sqrt (r_axis.x * r_axis.x + r_axis.y * r_axis.y + r_axis.z * r_axis.z);
      double x = r_axis.x * s;
      double y = r_axis.y * s;
      double z = r_axis.z * s;
      double w = cos (0.5 * angle);

      // Convert the quaternion to a 3x3 matrix
      double ww = w * w; double xx = x * x; double yy = y * y; double zz = z * z;
      double wx = w * x; double wy = w * y; double wz = w * z;
      double xy = x * y; double xz = x * z; double yz = y * z;

      transformation (0, 0) = xx - yy - zz + ww; transformation (0, 1) = 2*(xy - wz);       transformation (0, 2) = 2*(xz + wy);       transformation (0, 3) = tx;
      transformation (1, 0) = 2*(xy + wz);       transformation (1, 1) = -xx + yy -zz + ww; transformation (1, 2) = 2*(yz - wx);       transformation (1, 3) = ty;
      transformation (2, 0) = 2*(xz - wy);       transformation (2, 1) = 2*(yz + wx);       transformation (2, 2) = -xx -yy + zz + ww; transformation (2, 3) = tz;
      transformation (3, 0) = 0;                 transformation (3, 1) = 0;                 transformation (3, 2) = 0;                 transformation (3, 3) = 1;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Convert an axis-angle representation to a 3x3 rotation matrix
      * \note The formula is given by: A = I * cos (th) + ( 1 - cos (th) ) * axis * axis' - E * sin (th), where 
      * E = [0 -axis.z axis.y; axis.z 0 -axis.x; -axis.y axis.x 0]
      * \param axis the axis
      * \param angle the angle
      * \param rotation the resultant rotation
      */
    void
      convertAxisAngleToRotationMatrix (robot_msgs::Point32 axis, double angle, Eigen::Matrix3d &rotation)
    {
      double cos_a = cos (angle);
      double sin_a = sin (angle);
      double cos_a_m = 1.0 - cos_a;

      double a_xy = axis.x * axis.y * cos_a_m;
      double a_xz = axis.x * axis.z * cos_a_m;
      double a_yz = axis.y * axis.z * cos_a_m;

      double s_x = sin_a * axis.x;
      double s_y = sin_a * axis.y;
      double s_z = sin_a * axis.z;

      rotation (0, 0) = cos_a + axis.x * axis.x * cos_a_m;
      rotation (0, 1) = a_xy - s_z;
      rotation (0, 2) = a_xz + s_y;
      rotation (1, 0) = a_xy + s_z;
      rotation (1, 1) = cos_a + axis.y * axis.y * cos_a_m;
      rotation (1, 2) = a_yz - s_x;
      rotation (2, 0) = a_xz - s_y;
      rotation (2, 1) = a_yz + s_x;
      rotation (2, 2) = cos_a + axis.z * axis.z * cos_a_m;
    }
    
  }
}
