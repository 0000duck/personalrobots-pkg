/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/** \author Mrinal Kalakrishnan */

#ifndef DISTANCE_FIELD_H_
#define DISTANCE_FIELD_H_

#include <distance_field/voxel_grid.h>
#include <LinearMath/btVector3.h>
#include <vector>
#include <list>
#include <ros/ros.h>

namespace distance_field
{

/**
 * \brief Structure that holds voxel information for the DistanceField
 */
struct DistanceFieldVoxel
{
  DistanceFieldVoxel();
  DistanceFieldVoxel(int distance_sq);

  int distance_square_;         /**< Squared distance from the closest obstacle */
  int location_[3];             /**< Grid location of this voxel */
  int closest_point_[3];        /**< Closes obstacle from this voxel */
  int update_direction_;        /**< Direction from which this voxel was updated */

  static const int UNINITIALIZED=-1;
};

/**
 * \brief A VoxelGrid that can convert a set of obstacle points into a distance field.
 *
 * It computes the distance transform of the input points, and stores the distance to
 * the closest obstacle in each voxel. Also available is the location of the closest point,
 * and the gradient of the field at a point. Expansion of obstacles is performed upto a given
 * radius.
 */
class DistanceField: public VoxelGrid<DistanceFieldVoxel>
{
public:

  /**
   * \brief Constructor for the DistanceField
   */
  DistanceField(double size_x, double size_y, double size_z, double resolution,
      double origin_x, double origin_y, double origin_z, double max_distance);

  virtual ~DistanceField();

  /**
   * \brief Add (and expand) a set of points to the distance field
   */
  void addPointsToField(std::vector<btVector3> points);

  /**
   * \brief Resets the distance field to the max_distance
   */
  void reset();

  /**
   * Publishes an iso-surface of the given radius as visualization markers for rviz
   */
  void visualize(double radius, std::string frame_id, ros::Time stamp);

private:
  std::vector<std::vector<DistanceFieldVoxel*> > bucket_queue_;
  double max_distance_;
  int max_distance_sq_;
  ros::Publisher pub_viz_;

  // neighborhoods:
  // [0] - for expansion of d=0
  // [1] - for expansion of d>=1
  // Under this, we have the 27 directions
  // Then, a list of neighborhoods for each direction
  std::vector<std::vector<std::vector<std::vector<int> > > > neighborhoods_;

  std::vector<std::vector<int> > direction_number_to_direction_;

  int getDirectionNumber(int dx, int dy, int dz) const;
  void initNeighborhoods();
  static int eucDistSq(int* point1, int* point2);
};

////////////////////////// inline functions follow ////////////////////////////////////////

inline DistanceFieldVoxel::DistanceFieldVoxel(int distance_sq):
  distance_square_(distance_sq)
{
    for (int i=0; i<3; i++)
      closest_point_[i] = DistanceFieldVoxel::UNINITIALIZED;

}

inline DistanceFieldVoxel::DistanceFieldVoxel()
{
}

}

#endif /* DISTANCE_FIELD_H_ */