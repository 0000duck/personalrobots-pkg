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

#include <object_segmentation/image_stereo/table_object_rf.h>

using namespace std;

const std::string TableObjectRF::CHANNEL_ARRAY_WIDTH = "array_width";
const std::string TableObjectRF::CHANNEL_ARRAY_HEIGHT = "array_height";
const std::string TableObjectRF::CHANNEL_LABEL = "label";

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
TableObjectRF::TableObjectRF()
{
  vector<Descriptor3D*> node_feature_descriptors;
  vector<vector<Descriptor3D*> > clique_set_feature_descriptors;
  vector<vector<pair<bool, point_cloud_clustering::PointCloudClustering*> > >
      clique_set_clusterings;

  // -----------------------------------------
  // Node Features
  SpectralAnalysis* sa_nodes = new SpectralAnalysis(0.0254); // 0.0254 = 1 inch
  ShapeSpectral* ss_nodes = new ShapeSpectral(*sa_nodes);
  OrientationNormal* on_nodes = new OrientationNormal(0, 0, 1.0, *sa_nodes);
  //
  node_feature_descriptors.push_back(ss_nodes);
  node_feature_descriptors.push_back(on_nodes);

  // -----------------------------------------
  // Clique-set 0 features
  SpectralAnalysis* sa_cluster_cs0 = new SpectralAnalysis(-1);
  SpectralAnalysis* sa_radius_cs0 = new SpectralAnalysis(0.4445); // 1.75 inch = 0.04445 meters
  ShapeSpectral* ss_cluster_cs0 = new ShapeSpectral(*sa_cluster_cs0);
  ShapeSpectral* ss_radius_cs0 = new ShapeSpectral(*sa_radius_cs0);
  OrientationNormal* on_cluster_cs0 = new OrientationNormal(0, 0, 1.0, *sa_cluster_cs0);
  OrientationNormal* on_radius_cs0 = new OrientationNormal(0, 0, 1.0, *sa_radius_cs0);
  // 0.5 inch = 0.0127 m
  SpinImageCustom* sic_cs0 = new SpinImageCustom(0, 0, 1.0, 0.0127, 0.0127, 5, 5, false);
  SpinImageNormal* sin_cs0 = new SpinImageNormal(0.0127, 0.0127, 5, 5, false, *sa_cluster_cs0);
  BoundingBoxSpectral* bbs_cluster_cs0 = new BoundingBoxSpectral(-1.0, *sa_cluster_cs0);
  BoundingBoxSpectral* bbs_radius_cs0 = new BoundingBoxSpectral(-1.0, *sa_radius_cs0);
  vector<Descriptor3D*> cs0_feature_descriptors;
  cs0_feature_descriptors.push_back(ss_cluster_cs0);
  cs0_feature_descriptors.push_back(ss_radius_cs0);
  cs0_feature_descriptors.push_back(on_cluster_cs0);
  cs0_feature_descriptors.push_back(on_radius_cs0);
  cs0_feature_descriptors.push_back(sic_cs0);
  cs0_feature_descriptors.push_back(sin_cs0);
  cs0_feature_descriptors.push_back(bbs_cluster_cs0);
  cs0_feature_descriptors.push_back(bbs_radius_cs0);
  //
  clique_set_feature_descriptors.push_back(cs0_feature_descriptors);

  // -----------------------------------------
  // Clique-set 0 clustering (kmeans)
  point_cloud_clustering::KMeans* kmeans0_cs0 = new point_cloud_clustering::KMeans(0.005, 2);
  point_cloud_clustering::KMeans* kmeans1_cs0 = new point_cloud_clustering::KMeans(0.008, 2);
  vector<pair<bool, point_cloud_clustering::PointCloudClustering*> > cs0_clusterings(2);
  cs0_clusterings[0].first = true; // true indicates to cluster over only the nodes
  cs0_clusterings[0].second = kmeans0_cs0;
  cs0_clusterings[1].first = true; // true indicates to cluster over only the nodes
  cs0_clusterings[1].second = kmeans1_cs0;
  //
  clique_set_clusterings.push_back(cs0_clusterings);

  // -----------------------------------------
  // Clique-set 1 features (edges)
  vector<Descriptor3D*> cs1_feature_descriptors; // intentionally empty since using edges
  clique_set_feature_descriptors.push_back(cs1_feature_descriptors);

  // -----------------------------------------
  // Clique-set 1 clustering (edges)
  // 0.5 inch = 0.0127m
  point_cloud_clustering::PairwiseNeighbors* pn_cs1 =
      new point_cloud_clustering::PairwiseNeighbors(0.0127, 3);
  vector<pair<bool, point_cloud_clustering::PointCloudClustering*> > cs1_clusterings(1);
  cs1_clusterings[0].first = true; // true indicates to cluster over only the nodes
  cs1_clusterings[0].second = pn_cs1;
  clique_set_clusterings.push_back(cs1_clusterings);

  // -----------------------------------------
  // Initialize RFCreator3D with the above features and clustering
  rf_creator_3d_ = new RFCreator3D(node_feature_descriptors, clique_set_feature_descriptors,
      clique_set_clusterings);

  // 0.3 inch = 0.00762 m cells
  voxel_x_ = 0.00762;
  voxel_y_ = 0.00762;
  voxel_z_ = 0.00762;
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
boost::shared_ptr<RandomField> TableObjectRF::createRandomField(const string& fname_image,
                                                                const string& fname_pcd,
                                                                const float yaw,
                                                                const float pitch,
                                                                const float roll)
{
  // --------------------------------------------------
  // Load point cloud and image
  IplImage* image = NULL;
  sensor_msgs::PointCloud full_stereo_cloud;
  if (loadStereoImageCloud(fname_image, fname_pcd, &image, full_stereo_cloud) < 0)
  {
    abort();
  }

  // --------------------------------------------------
  // Downsample the point cloud
  // downsampling will put all invalid points into one point
  sensor_msgs::PointCloud ds_stereo_cloud;
  vector<unsigned int> ds_labels;
  vector<pair<unsigned int, unsigned int> > ds_img_coords;
  downsampleStereoCloud(full_stereo_cloud, ds_stereo_cloud, voxel_x_, voxel_y_, voxel_z_,
      ds_labels, ds_img_coords);

  // --------------------------------------------------
  // Rotate point cloud
  rotatePointCloud(ds_stereo_cloud, yaw, pitch, roll);

  // --------------------------------------------------
  // Compute image features for each downsampled point
  vector<vector<float> > ds_img_features;
  createImageFeatures(*image, ds_img_coords, ds_img_features);
  rf_creator_3d_->addExternalNodeFeatures(ds_img_features);

  // --------------------------------------------------
  // Create random field from point cloud only
  boost::shared_ptr<RandomField> created_rf = rf_creator_3d_->createRandomField(ds_stereo_cloud,
      ds_labels, true);

  cvReleaseImage(&image);
  return created_rf;
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
int TableObjectRF::loadStereoImageCloud(const string& fname_image,
                                        const string& fname_pcd,
                                        IplImage** image,
                                        sensor_msgs::PointCloud& stereo_cloud)
{
  ROS_INFO("Loading image....");
  *image = cvLoadImage(fname_image.c_str(), CV_LOAD_IMAGE_GRAYSCALE);
  if (image == NULL)
  {
    return -1;
  }
  ROS_INFO("done.");

  ROS_INFO("Loading point cloud....");
  if (cloud_io::loadPCDFile(fname_pcd.c_str(), stereo_cloud) == -1)
  {
    cvReleaseImage(image);
    return -1;
  }
  ROS_INFO("done.");

  // verify dimensions are contained in the pcd file
  if (cloud_geometry::getChannelIndex(stereo_cloud, TableObjectRF::CHANNEL_ARRAY_WIDTH) < 0
      || cloud_geometry::getChannelIndex(stereo_cloud, TableObjectRF::CHANNEL_ARRAY_HEIGHT) < 0)
  {
    ROS_ERROR("Could not find stereo image dimensions from the cloud");
    cvReleaseImage(image);
    return -1;
  }

  return 0;
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
void TableObjectRF::downsampleStereoCloud(sensor_msgs::PointCloud& full_stereo_cloud,
                                          sensor_msgs::PointCloud& ds_stereo_cloud,
                                          const double voxel_x,
                                          const double voxel_y,
                                          const double voxel_z,
                                          vector<unsigned int>& ds_labels,
                                          vector<pair<unsigned int, unsigned int> >& ds_img_coords)
{
  // guaranteed to exist
  unsigned int width = full_stereo_cloud.channels[cloud_geometry::getChannelIndex(
      full_stereo_cloud, TableObjectRF::CHANNEL_ARRAY_WIDTH)].values[0];

  // not guaranteed to exist
  int label_channel = cloud_geometry::getChannelIndex(full_stereo_cloud,
      TableObjectRF::CHANNEL_LABEL);

  // create kdtree of original point cloud to find downsample's closest point
  cloud_kdtree::KdTreeANN full_cloud_kdtree(full_stereo_cloud);

  // Downsample the cloud
  geometry_msgs::Point voxel_dim;
  voxel_dim.x = voxel_x;
  voxel_dim.y = voxel_y;
  voxel_dim.z = voxel_z;
  cloud_geometry::downsamplePointCloud(full_stereo_cloud, ds_stereo_cloud, voxel_dim);
  int nbr_downsampled_pts = ds_stereo_cloud.points.size();

  ds_labels.assign(nbr_downsampled_pts, RandomField::UNKNOWN_LABEL);
  ds_img_coords.clear();

  // For each downsampled point, find its closest point in the original full point cloud
  // and extract the point's image coordinates (and label if present)
  pair<unsigned int, unsigned int> empty_coords(0, 0);
  ds_img_coords.assign(nbr_downsampled_pts, empty_coords);
#pragma omp parallel for
  for (int i = 0 ; i < nbr_downsampled_pts ; i++)
  {
    const geometry_msgs::Point32& curr_ds_pt = ds_stereo_cloud.points[i];

    // find the voxel's centroid closest point in the full point cloud
    vector<int> k_indices;
    vector<float> k_distances;
    full_cloud_kdtree.nearestKSearch(curr_ds_pt, 1, k_indices, k_distances);
    const unsigned int closest_full_idx = static_cast<unsigned int> (k_indices.at(0));

    // associate downsample index with image coords
    // lookup the full point cloud's image (x,y) coords
    // (OpenCV: x = left to right, y = top to bottom)
    unsigned int y = closest_full_idx / width;
    unsigned int x = closest_full_idx % width;
    ds_img_coords[i].first = x;
    ds_img_coords[i].second = y;

    // if present, assign the down sampled point the label of the closest point in the original point cloud
    if (label_channel >= 0)
    {
      ds_labels[i] = full_stereo_cloud.channels[label_channel].values[closest_full_idx];
    }
  }
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
void TableObjectRF::createImageFeatures(IplImage& image, const vector<pair<unsigned int,
    unsigned int> >& ds_img_coords, vector<vector<float> >& ds_img_features)
{
  unsigned int nbr_image_features = 2;
  unsigned int nbr_pts = ds_img_coords.size();

  // TODO change this when doing features that can fail
  ds_img_features.assign(nbr_pts, vector<float> (nbr_image_features, 0.0));

  for (unsigned int i = 0 ; i < nbr_pts ; i++)
  {
    const pair<unsigned int, unsigned int>& curr_img_coords = ds_img_coords[i];
    unsigned int w = curr_img_coords.first;
    unsigned int h = curr_img_coords.second;

    // height and value of pixel
    ds_img_features[i][0] = h;
    unsigned char* tempo = (unsigned char*)(image.imageData + (h * image.widthStep));
    ds_img_features[i][1] = static_cast<float>(tempo[w]);
  }
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
void TableObjectRF::rotatePointCloud(sensor_msgs::PointCloud& pc_in,
                                     const float yaw,
                                     const float pitch,
                                     const float roll)
{
  btTransform transform(btQuaternion(yaw, pitch, roll), btVector3(0, 0, 0));
  btVector3 curr_pt(0.0, 0.0, 0.0);
  btVector3 rotated_pt(0.0, 0.0, 0.0);

  unsigned int nbr_pts = pc_in.points.size();
  for (unsigned int i = 0 ; i < nbr_pts ; i++)
  {
    curr_pt.setX(pc_in.points[i].x);
    curr_pt.setY(pc_in.points[i].y);
    curr_pt.setZ(pc_in.points[i].z);
    btVector3 rotated_pt = transform * curr_pt;
    pc_in.points[i].x = rotated_pt.getX();
    pc_in.points[i].y = rotated_pt.getY();
    pc_in.points[i].z = rotated_pt.getZ();
  }
}