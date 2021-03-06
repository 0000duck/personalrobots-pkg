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

#include <point_cloud_clustering/pairwise_neighbors.h>

using namespace std;

// --------------------------------------------------------------
/* See function definition */
// --------------------------------------------------------------
point_cloud_clustering::PairwiseNeighbors::PairwiseNeighbors(double neighbor_radius,
                                                             unsigned int max_nbr_neighbors)
{
  if (neighbor_radius < 0.0)
  {
    throw "point_cloud_clustering::PairwiseNeighbors() invalid radius, must be between positive";
  }

  neighbor_radius_ = neighbor_radius;
  max_nbr_neighbors_ = max_nbr_neighbors;
}

// --------------------------------------------------------------
/* See function definition
 * Important: if you change indices_to_cluster from a set to anything else,
 *            must modify adjacency list construction below */
// --------------------------------------------------------------
int point_cloud_clustering::PairwiseNeighbors::cluster(const sensor_msgs::PointCloud& pt_cloud,
                                                       cloud_kdtree::KdTree& pt_cloud_kdtree,
                                                       const set<unsigned int>& indices_to_cluster,
                                                       map<unsigned int, vector<int> >& created_clusters)
{
  created_clusters.clear();

  // 0 indicates to link to all neighbors
  bool link_to_all_neighbors = max_nbr_neighbors_ == 0;

  // -------------------------------------------------
  // Create adjacency list where
  // adj_list: i --> [point i's neighbors j], such that i < j
  map<unsigned int, set<unsigned int> > adj_list;

  // -------------------------------------------------
  // Iterate over each index, find neighboring points, and randomly link to them
  const unsigned int nbr_total_pts = pt_cloud.points.size();
  for (set<unsigned int>::const_iterator iter_indices_to_cluster = indices_to_cluster.begin() ; iter_indices_to_cluster
      != indices_to_cluster.end() ; iter_indices_to_cluster++)
  {
    // -----------------------
    // Retrieve next index to crate edge from
    const unsigned int curr_pt_cloud_idx = *iter_indices_to_cluster;
    if (curr_pt_cloud_idx >= nbr_total_pts)
    {
      ROS_ERROR("Invalid index to cluster: %u out of %u", curr_pt_cloud_idx, nbr_total_pts);
      return -1;
    }

    // -----------------------
    // STL set contains unique values, so next entry in adj doesnt exist
    adj_list[curr_pt_cloud_idx] = set<unsigned int> ();

    // -----------------------
    // Find valid neighboring candidate points to link to
    vector<unsigned int> valid_neighbor_indices;
    findRadiusNeighbors(pt_cloud_kdtree, curr_pt_cloud_idx, neighbor_radius_, indices_to_cluster,
        valid_neighbor_indices);

    // -----------------------
    // If NOT linking to all neighbors, randomly pick max_nbr_neighbors_
    if (link_to_all_neighbors == false)
    {
      random_shuffle(valid_neighbor_indices.begin(), valid_neighbor_indices.end());
      valid_neighbor_indices.resize(max_nbr_neighbors_);
    }
    unsigned int nbr_valid_neighbors = valid_neighbor_indices.size();

    // -----------------------
    // Now link to random neighbors within radius
    for (unsigned int i = 0 ; i < nbr_valid_neighbors ; i++)
    {
      // retrieve neighboring index
      const unsigned int curr_neighbor_idx = valid_neighbor_indices[i];

      // avoid self-edges
      if (curr_neighbor_idx == curr_pt_cloud_idx)
      {
        continue;
      }

      // define edge (a,b) st. a < b
      const unsigned int a = std::min(curr_pt_cloud_idx, curr_neighbor_idx);
      const unsigned int b = std::max(curr_pt_cloud_idx, curr_neighbor_idx);
      adj_list[a].insert(b); // a guaranteed to exist since iterating in ascending order with set
    }
  }

  // -------------------------------------------------
  // Create clusters (edges) from adjacency list
  unsigned int curr_cluster_label = starting_label_;
  for (map<unsigned int, set<unsigned int> >::iterator iter_adj_list = adj_list.begin() ; iter_adj_list
      != adj_list.end() ; iter_adj_list++)
  {
    // i
    const unsigned int curr_source_idx = iter_adj_list->first;
    // i's neighbors j
    const set<unsigned int>& curr_neighbors = iter_adj_list->second;

    // -----------------------
    // Iterate and create edges from i to each neighbor j
    for (set<unsigned int>::const_iterator iter_neighbors = curr_neighbors.begin() ; iter_neighbors
        != curr_neighbors.end() ; iter_neighbors++)
    {
      // j
      const unsigned int curr_target_idx = *iter_neighbors;

      // The "cluster" is an edge with 2 points
      created_clusters[curr_cluster_label] = vector<int> (2);

      // Create edges such that first point is always lower (z-coord) than the second point
      if (pt_cloud.points[curr_source_idx].z < pt_cloud.points[curr_target_idx].z)
      {
        created_clusters[curr_cluster_label][0] = static_cast<int> (curr_source_idx);
        created_clusters[curr_cluster_label][1] = static_cast<int> (curr_target_idx);
      }
      else
      {
        created_clusters[curr_cluster_label][0] = static_cast<int> (curr_target_idx);
        created_clusters[curr_cluster_label][1] = static_cast<int> (curr_source_idx);
      }
      curr_cluster_label++;
    }
  }

  return 0;
}
