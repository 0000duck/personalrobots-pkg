#ifndef __TABLE_OBJECT_RF_H__
#define __TABLE_OBJECT_RF_H__
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage
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

#include <string>
#include <map>
#include <vector>
#include <set>

#include <boost/shared_ptr.hpp>

#include <ros/ros.h>

#include "opencv/cxcore.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "opencv/cvaux.hpp"

#include <sensor_msgs/PointCloud.h>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/Point32.h>

#include <point_cloud_mapping/kdtree/kdtree_ann.h>
#include <point_cloud_mapping/geometry/point.h>
#include <point_cloud_mapping/cloud_io.h>

#include <point_cloud_clustering/all_clusterings.h>

#include <descriptors_3d/all_descriptors.h>
#include <descriptors_2d/descriptors_2d.h>
#include <functional_m3n/random_field.h>

#include <object_segmentation/util/rf_creator_3d.h>

#include <LinearMath/btTransform.h>
#include <LinearMath/btVector3.h>

class TableObjectRF
{
  public:
    static const std::string CHANNEL_ARRAY_WIDTH;
    static const std::string CHANNEL_ARRAY_HEIGHT;
    static const std::string CHANNEL_LABEL;

    TableObjectRF();

    // label_to_ignore will be ignored if value < 0
    boost::shared_ptr<RandomField> createRandomField(const std::string& fname_image,
                                                     const std::string& fname_pcd,
                                                     const float yaw,
                                                     const float pitch,
                                                     const float roll,
                                                     const int label_to_ignore);

  private:
    int loadStereoImageCloud(const std::string& fname_image,
                             const std::string& fname_pcd,
                             IplImage** image,
                             sensor_msgs::PointCloud& stereo_cloud);

    // ds_stereo_cloud loses all channels
    // label_to_ignore will be ignored if value < 0
    // will set ds_labels[i] = UNKNOWN if original label in full_stereo_cloud equals label_to_ignore
    void
    downsampleStereoCloud(sensor_msgs::PointCloud& full_stereo_cloud,
                          sensor_msgs::PointCloud& ds_stereo_cloud,
                          const double voxel_x,
                          const double voxel_y,
                          const double voxel_z,
                          const int label_to_ignore,
                          std::vector<unsigned int>& ds_labels,
                          std::vector<std::pair<unsigned int, unsigned int> >& ds_img_coords,
                          std::set<unsigned int>& ignore_ds_indices);

    // ds_img_features.size() = ds_img_coords.size() - ignore_ds_indices.size();
    void
    createImageFeatures(IplImage& image,
                        const std::set<unsigned int>& ignore_ds_indices,
                        const std::vector<std::pair<unsigned int, unsigned int> >& ds_img_coords,
                        std::vector<std::vector<float> >& ds_img_features);

    void rotatePointCloud(sensor_msgs::PointCloud& pc_in,
                          const float yaw,
                          const float pitch,
                          const float roll);

    RFCreator3D* rf_creator_3d_;
    double voxel_x_;
    double voxel_y_;
    double voxel_z_;
};

#endif
