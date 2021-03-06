#ifndef __PCC_KMEANS_H__
#define __PCC_KMEANS_H__
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

#include <stdlib.h>

#include <set>
#include <map>
#include <vector>

#include <sensor_msgs/PointCloud.h>

#include <point_cloud_mapping/kdtree/kdtree.h>

#include <point_cloud_clustering/point_cloud_clustering.h>

#include <point_cloud_clustering/3rd_party/KMeans.h>

// --------------------------------------------------------------
/*!
 * \file kmeans.h
 *
 * \brief KMeans performs basic k-means clustering on the point
 *        cloud
 */
// --------------------------------------------------------------

namespace point_cloud_clustering
{
  // --------------------------------------------------------------
  /*!
   * \brief A KMeans clustering performs k-means clustering on the point
   *        cloud where the features are the x,y,z coordinates.
   *
   * This class wraps around the k-means++ approach as implemented in: \n
   * k-means++: The advantages of careful seeding \n
   * D. Arthur and S. Vassilvitskii \n
   * SODA 2007 \n
   * http://www.stanford.edu/~darthur/kmpp.zip
   */
  // --------------------------------------------------------------
  class KMeans: public PointCloudClustering
  {
    public:
      // --------------------------------------------------------------
      /*!
       * \brief Instantiates the k-means++ clustering
       *
       * \param k_factor Defines the number of clusters to produce where
       *                 k = k_factor * [number of points to cluster over]
       * \param nbr_attempts
       */
      // --------------------------------------------------------------
      KMeans(double k_factor, unsigned int nbr_attempts);

      // --------------------------------------------------------------
      /*!
       * \brief Note: KMeans does not require efficient neighborhood search
       *              so an empty kdtree can be passed
       *
       * \see PointCloudClustering::cluster
       */
      // --------------------------------------------------------------
      virtual int cluster(const sensor_msgs::PointCloud& pt_cloud,
                          cloud_kdtree::KdTree& pt_cloud_kdtree,
                          const std::set<unsigned int>& indices_to_cluster,
                          std::map<unsigned int, std::vector<int> >& created_clusters);

    private:
      double k_factor_;
      unsigned int nbr_attempts_;
  };
}
#endif
