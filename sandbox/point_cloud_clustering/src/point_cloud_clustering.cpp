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

#include <point_cloud_clustering/point_cloud_clustering.h>

using namespace std;

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
void point_cloud_clustering::PointCloudClustering::computeClusterCentroids(const sensor_msgs::PointCloud& pt_cloud,
                                                                           const map<unsigned int,
                                                                               vector<int> >& clusters,
                                                                           map<unsigned int,
                                                                               vector<float> >& cluster_centroids)
{
  cluster_centroids.clear();

  // Iterate over clusters (cluster_label --> [point cloud indices]
  for (map<unsigned int, vector<int> >::const_iterator iter_clusters = clusters.begin() ; iter_clusters
      != clusters.end() ; iter_clusters++)
  {
    // ----------------
    // retrieve point indices of current cluster
    const vector<int>& curr_cluster_pt_indices = iter_clusters->second;
    const unsigned int curr_cluster_nbr_pts = curr_cluster_pt_indices.size();

    // ----------------
    // accumulate sum coordinates for each point in cluster
    vector<float> curr_centroid(3, 0.0);
    for (unsigned int i = 0 ; i < curr_cluster_nbr_pts ; i++)
    {
      const geometry_msgs::Point32& curr_pt = pt_cloud.points.at(curr_cluster_pt_indices[i]);
      curr_centroid[0] += curr_pt.x;
      curr_centroid[1] += curr_pt.y;
      curr_centroid[2] += curr_pt.z;
    }

    // ----------------
    // normalize by number of points
    if (curr_cluster_nbr_pts > 0)
    {
      for (unsigned int i = 0 ; i < 3 ; i++)
      {
        curr_centroid[i] /= static_cast<float> (curr_cluster_nbr_pts);
      }
    }

    // ----------------
    // insert into results
    const unsigned int curr_cluster_label = iter_clusters->first;
    cluster_centroids.insert(pair<unsigned int, vector<float> > (curr_cluster_label, curr_centroid));
  }

  return;
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
point_cloud_clustering::PointCloudClustering::PointCloudClustering()
{
  starting_label_ = 0;
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
point_cloud_clustering::PointCloudClustering::~PointCloudClustering()
{
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
int point_cloud_clustering::PointCloudClustering::cluster(const sensor_msgs::PointCloud& pt_cloud,
                                                          cloud_kdtree::KdTree& pt_cloud_kdtree,
                                                          std::map<unsigned int, std::vector<int> >& created_clusters)
{
  // Create set of all indices in the point cloud
  set<unsigned int> all_indices;
  const unsigned int nbr_pts = pt_cloud.points.size();
  pair<set<unsigned int>::iterator, bool> ret = all_indices.insert(0);
  for (unsigned int i = 1 ; i < nbr_pts ; i++)
  {
    const set<unsigned int>::iterator& next_position = ret.first;
    ret.first = all_indices.insert(next_position, i);
  }

  return cluster(pt_cloud, pt_cloud_kdtree, all_indices, created_clusters);
}

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
void point_cloud_clustering::PointCloudClustering::findRadiusNeighbors(cloud_kdtree::KdTree& pt_cloud_kdtree,
                                                                       unsigned int index,
                                                                       double radius,
                                                                       const std::set<unsigned int>& indices_to_cluster,
                                                                       std::vector<unsigned int>& neighbor_indices)
{
  neighbor_indices.clear();

  // Perform range search over entire point cloud
  vector<int> k_indices;
  vector<float> k_distances;
  pt_cloud_kdtree.radiusSearch(static_cast<int> (index), radius, k_indices, k_distances);

  // Only find neighbors from range search that are "valid"
  const unsigned int nbr_k_neighbors = k_indices.size();
  neighbor_indices.reserve(nbr_k_neighbors);
  for (unsigned int i = 0 ; i < nbr_k_neighbors ; i++)
  {
    // check if the neighbor point is "valid"
    const unsigned int neighbor_idx = static_cast<unsigned int> (k_indices[i]);
    if (indices_to_cluster.count(neighbor_idx) != 0)
    {
      neighbor_indices.push_back(neighbor_idx);
    }
  }
  return;
}
