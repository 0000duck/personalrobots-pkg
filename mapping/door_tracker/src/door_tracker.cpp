//Software License Agreement (BSD License)

//Copyright (c) 2008, Willow Garage, Inc.
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above
//   copyright notice, this list of conditions and the following
//   disclaimer in the documentation and/or other materials provided
//   with the distribution.
// * Neither the name of Willow Garage, Inc. nor the names of its
//   contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.

/**
@mainpage

@htmlinclude manifest.html

\author Sachin Chitta

@b door_tracker tracks a door that is in front of the robot

 **/

#include <ros/node.h>
// ROS messages
#include <door_handle_detector/DoorsDetector.h>

#include <sensor_msgs/PointCloud.h>
#include <geometry_msgs/Polygon.h>
#include <mapping_msgs/PolygonalMap.h>

#include <geometry_msgs/Point32.h>
#include <door_msgs/Door.h>
#include <visualization_msgs/Marker.h>
#include <std_msgs/String.h>

// Sample Consensus
#include <point_cloud_mapping/sample_consensus/sac.h>
#include <point_cloud_mapping/sample_consensus/msac.h>
#include <point_cloud_mapping/sample_consensus/ransac.h>
#include <point_cloud_mapping/sample_consensus/lmeds.h>
#include <point_cloud_mapping/sample_consensus/sac_model_line.h>
#include <point_cloud_mapping/sample_consensus/sac_model_plane.h>
#include <point_cloud_mapping/kdtree/kdtree_ann.h>

// Cloud geometry
#include <point_cloud_mapping/geometry/areas.h>
#include <point_cloud_mapping/geometry/point.h>
#include <point_cloud_mapping/geometry/distances.h>
#include <point_cloud_mapping/geometry/nearest.h>
#include <point_cloud_mapping/geometry/statistics.h>

#include <tf/transform_listener.h>
#include <tf/message_notifier.h>

#include <angles/angles.h>

#include <sys/time.h>

// Clouds and scans
#include <sensor_msgs/LaserScan.h>
#include <laser_geometry/laser_geometry.h>

//Filters
#include "filters/filter_chain.h"

using namespace std;
using namespace sensor_msgs;
using namespace mapping_msgs;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comparison operator for a vector of vectors
inline bool
  compareRegions (const std::vector<int> &a, const std::vector<int> &b)
{
  return (a.size () < b.size ());
}

class DoorTracker
{
  public:
    ros::NodeHandle node_handle_;
    sensor_msgs::PointCloud cloud_;
    laser_geometry::LaserProjection projector_; // Used to project laser scans into point clouds
    tf::TransformListener tf_;
    boost::scoped_ptr<tf::MessageNotifier<sensor_msgs::LaserScan> > message_notifier_;
    door_msgs::Door door_msg_;
    door_msgs::Door door_msg_in_;
    boost::mutex door_msg_mutex_ ;
    boost::mutex cloud_msg_mutex_ ;
    bool active_;
    filters::FilterChain<sensor_msgs::LaserScan> filter_chain_;
    tf::Stamped<tf::Pose> global_pose_;

    double global_yaw_;
    double global_x_;
    double global_y_;
    bool done_detection_;
    double euclidean_cluster_angle_tolerance_;
    double  euclidean_cluster_min_pts_;               // 1000 points
    double  euclidean_cluster_distance_tolerance_;               // 5 cm

    ros::Publisher viz_pub_;
    ros::Publisher door_pub_;
    ros::ServiceServer door_srv_;
    ros::Subscriber activate_sub_;

    /********** Parameters that need to be gotten from the param server *******/
    std::string door_msg_topic_, base_laser_topic_,fixed_frame_;
    int sac_min_points_per_model_;
    double sac_distance_threshold_;
    double eps_angle_, frame_multiplier_;
    int sac_min_points_left_;
    double door_min_width_, door_max_width_;
    std_msgs::String activate_msg_;
    bool publish_all_candidates_;
    int num_clouds_received_;
    bool continuous_detection_;

    bool first_time_;

    DoorTracker():message_notifier_(NULL), filter_chain_("sensor_msgs::LaserScan")
    {
      num_clouds_received_ = 0;
      continuous_detection_ = false;
      active_ = false;
      activate_sub_ = node_handle_.subscribe("~activate",1,&DoorTracker::activate,this);
      ROS_DEBUG("Started door tracker");
      //Laser Scan Filtering
      filter_chain_.configure("~filters");
      done_detection_ = true;
      euclidean_cluster_angle_tolerance_    = angles::from_degrees (25.0);
      euclidean_cluster_min_pts_            = 20;               // 1000 points
      euclidean_cluster_distance_tolerance_ = 0.03;               // 5 cm

      node_handle_.param<std::string>("~p_door_msg_topic_", door_msg_topic_, "door_message");                              // 10 degrees
      node_handle_.param<std::string>("~p_base_laser_topic_", base_laser_topic_, "/base_scan");                              // 10 degrees

      node_handle_.param ("~p_sac_min_points_per_model", sac_min_points_per_model_, 50);  // 100 points at high resolution
      node_handle_.param ("~p_sac_distance_threshold", sac_distance_threshold_, 0.03);     // 3 cm
      node_handle_.param ("~p_eps_angle_", eps_angle_, 10.0);                              // 10 degrees
      node_handle_.param ("~p_frame_multiplier_", frame_multiplier_,6.0);
      node_handle_.param ("~p_sac_min_points_left", sac_min_points_left_, 10);
      node_handle_.param ("~p_door_min_width", door_min_width_, 0.8);                    // minimum width of a door: 0.8m
      node_handle_.param ("~p_door_max_width", door_max_width_, 0.9);                    // maximum width of a door: 1.4m
      node_handle_.param("~p_fixed_frame", fixed_frame_, string("map"));

      eps_angle_ = angles::from_degrees (eps_angle_);                      // convert to radians

      double tmp; int tmp2;
      node_handle_.param("~p_door_frame_p1_x", tmp, 0.5); door_msg_.frame_p1.x = tmp;
      node_handle_.param("~p_door_frame_p1_y", tmp, -0.5); door_msg_.frame_p1.y = tmp;
      node_handle_.param("~p_door_frame_p2_x", tmp, 0.5); door_msg_.frame_p2.x = tmp;
      node_handle_.param("~p_door_frame_p2_y", tmp, 0.5); door_msg_.frame_p2.y = tmp;
      node_handle_.param("~p_door_hinge" , tmp2, -1); door_msg_.hinge = tmp2;
      node_handle_.param("~p_door_rot_dir" , tmp2, -1); door_msg_.rot_dir = tmp2;
      door_msg_.header.frame_id = "base_link";
      node_handle_.param("~publish_all_candidates" , publish_all_candidates_, false);
      viz_pub_ = node_handle_.advertise<visualization_msgs::Marker>( "visualization_marker", 0 );
      door_pub_ = node_handle_.advertise<door_msgs::Door>( "~door_message", 0 );
      door_srv_ = node_handle_.advertiseService ("~doors_detector", &DoorTracker::detectDoorService, this);
      message_notifier_.reset(new tf::MessageNotifier<sensor_msgs::LaserScan> (tf_, boost::bind(&DoorTracker::laserCallBack, this, _1), base_laser_topic_, fixed_frame_, 1));
      message_notifier_->setTolerance(ros::Duration(.02));
      first_time_ = false;
    };

    ~DoorTracker()
    {
    }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Decompose a region of space into clusters based on the euclidean distance between points, and the normal
 * angular deviation
 * \NOTE: assumes normalized point normals !
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
    void findClusters(const sensor_msgs::PointCloud &points, const vector<int> &indices, double tolerance, vector<vector<int> > &clusters, int nx_idx, int ny_idx, int nz_idx, double eps_angle, unsigned int min_pts_per_cluster)
    {
      // Create a tree for these points
      cloud_kdtree::KdTree* tree = new cloud_kdtree::KdTreeANN (points, indices);

      int nr_points = indices.size ();
      // Create a bool vector of processed point indices, and initialize it to false
      vector<bool> processed;
      processed.resize (nr_points, false);

      vector<int> nn_indices;
      vector<float> nn_distances;
      // Process all points in the indices vector
      for (int i = 0; i < nr_points; i++)
      {
        if (processed[i])
          continue;

        vector<int> seed_queue;
        int sq_idx = 0;
        seed_queue.push_back (i);
//    double norm_a = 0.0;
//    if (nx_idx != -1)         // If we use normal indices...
//      norm_a = sqrt (points->channels[nx_idx].values[indices->at (i)] * points->channels[nx_idx].values[indices->at (i)] +
//                     points->channels[ny_idx].values[indices->at (i)] * points->channels[ny_idx].values[indices->at (i)] +
//                     points->channels[nz_idx].values[indices->at (i)] * points->channels[nz_idx].values[indices->at (i)]);
        processed[i] = true;
        while (sq_idx < (int)seed_queue.size ())
        {
          // Search for sq_idx
          tree->radiusSearch (seed_queue.at (sq_idx), tolerance, nn_indices, nn_distances);

          for (unsigned int j = 1; j < nn_indices.size (); j++)       // nn_indices[0] should be sq_idx
          {
            if (processed.at (nn_indices[j]))                         // Has this point been processed before ?
              continue;

            processed[nn_indices[j]] = true;
            if (nx_idx != -1)                                         // Are point normals present ?
            {
//          double norm_b = sqrt (points.channels[nx_idx].values[indices.at (nn_indices[j])] * points.channels[nx_idx].values[indices.at (nn_indices[j])] +
//                                points.channels[ny_idx].values[indices.at (nn_indices[j])] * points.channels[ny_idx].values[indices.at (nn_indices[j])] +
//                                points.channels[nz_idx].values[indices.at (nn_indices[j])] * points.channels[nz_idx].values[indices.at (nn_indices[j])]);
              // [-1;1]
              double dot_p = points.channels[nx_idx].values[indices.at (i)] * points.channels[nx_idx].values[indices.at (nn_indices[j])] +
                points.channels[ny_idx].values[indices.at (i)] * points.channels[ny_idx].values[indices.at (nn_indices[j])] +
                points.channels[nz_idx].values[indices.at (i)] * points.channels[nz_idx].values[indices.at (nn_indices[j])];
//          if ( acos (dot_p / (norm_a * norm_b)) < eps_angle)
              if ( fabs (acos (dot_p)) < eps_angle )
              {
                processed[nn_indices[j]] = true;
                seed_queue.push_back (nn_indices[j]);
              }
            }
            // If normal information is not present, perform a simple Euclidean clustering
            else
            {
              processed[nn_indices[j]] = true;
              seed_queue.push_back (nn_indices[j]);
            }
          }
          sq_idx++;
        }
        // If this queue is satisfactory, add to the clusters
        if (seed_queue.size () >= min_pts_per_cluster)
        {
          vector<int> r (seed_queue.size ());
          for (unsigned int j = 0; j < seed_queue.size (); j++)
            r[j] = indices.at (seed_queue[j]);

          sort (r.begin (), r.end ());
          r.erase (unique (r.begin (), r.end ()), r.end ());

          clusters.push_back (r);
        }
      }
      // Destroy the tree
      delete tree;
    }

    void start()
    {
      active_ = true;
    }

    void shutdown()
    {
        active_ = false;
        num_clouds_received_ = 0;
    }

    void updateGlobalPose()
    {
      tf::Stamped<tf::Pose> robotPose;
      robotPose.setIdentity();
      robotPose.frame_id_ = "base_link";
      robotPose.stamp_ = ros::Time();
      try{
        tf_.transformPose(fixed_frame_, robotPose, global_pose_);
      }
      catch(tf::LookupException& ex) {
        ROS_ERROR("No Transform available Error: %s\n", ex.what());
      }
      catch(tf::ConnectivityException& ex) {
        ROS_ERROR("Connectivity Error: %s\n", ex.what());
      }
      catch(tf::ExtrapolationException& ex) {
        ROS_ERROR("Extrapolation Error: %s\n", ex.what());
      }
      double useless_pitch, useless_roll, yaw;
      global_pose_.getBasis().getEulerZYX(yaw, useless_pitch, useless_roll);
      global_yaw_ = yaw;
      global_x_ = global_pose_.getOrigin().x();
      global_y_ = global_pose_.getOrigin().y();
    }

    void laserCallBack(const tf::MessageNotifier<sensor_msgs::LaserScan>::MessagePtr& scan_msg)
    {
      door_msgs::Door door_tmp;
      if(!active_)
      {
        ROS_DEBUG("Door tracker not active so returning");
        return;
      }
      if(!done_detection_)
      {
        ROS_DEBUG("Not done with detection so returning");
        return;
      }
      cloud_msg_mutex_.lock();
      done_detection_  = false;
      sensor_msgs::LaserScan filtered_scan;
      filter_chain_.update (*scan_msg, filtered_scan);// Transform into a PointCloud message
      int mask = laser_geometry::MASK_INTENSITY | laser_geometry::MASK_DISTANCE | laser_geometry::MASK_INDEX | laser_geometry::MASK_TIMESTAMP;
      try 
      {
        projector_.transformLaserScanToPointCloud(fixed_frame_, cloud_, filtered_scan, tf_, mask);
      }
      catch (tf::TransformException& e) {
        ROS_ERROR ("TF exception transforming scan to cloud: %s", e.what());
        cloud_msg_mutex_.unlock();
        return;
      }
      num_clouds_received_++;
      ROS_INFO("Received a point cloud with %d points in frame: %s",(int) cloud_.points.size(),cloud_.header.frame_id.c_str());
      if(cloud_.points.empty())
      {
        ROS_WARN("Received an empty point cloud");
        cloud_msg_mutex_.unlock();
        return;
      }
      if ((int)cloud_.points.size() < sac_min_points_per_model_)
      {
        cloud_msg_mutex_.unlock();
        return;
      }
      if(continuous_detection_) // do this on every laser callback
      {
        findDoor(door_tmp);
      }
      cloud_msg_mutex_.unlock();
    }

    void publishLine(const geometry_msgs::Point32 &min_p, const geometry_msgs::Point32 &max_p, const int &id, const std::string &frame_id)
    {
      visualization_msgs::Marker marker;
      marker.header.frame_id = frame_id;
      marker.header.stamp = ros::Time();
      marker.ns = "door_tracker";
      marker.id = id;
      marker.type = visualization_msgs::Marker::LINE_STRIP;
      marker.action = visualization_msgs::Marker::ADD;
      marker.pose.orientation.w = 1.0;
      marker.scale.x = 0.01;
      marker.scale.y = 0.1;
      marker.scale.z = 0.1;
      marker.color.a = 1.0;
      marker.color.b = 1.0;
      marker.set_points_size(2);
      marker.points[0].x = min_p.x;
      marker.points[0].y = min_p.y;
      marker.points[0].z = min_p.z;
      marker.points[1].x = max_p.x;
      marker.points[1].y = max_p.y;
      marker.points[1].z = max_p.z;
      ROS_DEBUG("Publishing line between: p1: %f %f %f, p2: %f %f %f",marker.points[0].x,marker.points[0].y,marker.points[0].z,marker.points[1].x,marker.points[1].y,marker.points[1].z);
      viz_pub_.publish(marker );
    }

    void transform2DInverse(const geometry_msgs::Point32 &fp_in, geometry_msgs::Point32 &fp_out, const double &robot_x, const double &robot_y, const double &robot_theta)
    {
      double cth = cos(robot_theta);
      double sth = sin(robot_theta);
      fp_out.x = fp_in.x*cth+fp_in.y*sth-robot_x*cth-robot_y*sth;
      fp_out.y = -fp_in.x*sth+fp_in.y*cth+robot_x*sth-robot_y*cth;
      return;
    }

    bool findDoor(door_msgs::Door &door_result)
    {
      ROS_DEBUG("Finding door");
      //cloud_msg_mutex_.lock();
      sensor_msgs::PointCloud cloud = cloud_;//create a local copy - should be fine since its a small scan from the base laser
      //cloud_msg_mutex_.unlock();

      updateGlobalPose();
      vector<int> indices;
      vector<int> possible_door_points;
      int inliers_size_max = 0;
      int inliers_size_max_index = -1;
      indices.resize(cloud.points.size());
      for(unsigned int i=0; i < cloud.points.size(); i++)      //Use all the indices
      {
        indices[i] = i;
      }

      // Find the dominant lines
      vector<vector<int> > inliers;
      vector<vector<double> > coeff;
      vector<geometry_msgs::Point32> line_segment_min;
      vector<geometry_msgs::Point32> line_segment_max;
      fitSACLines(&cloud,indices,inliers,coeff,line_segment_min,line_segment_max);
      if(publish_all_candidates_)
      {
        for(unsigned int i=0; i < inliers.size(); i++)
        {
          publishLine(line_segment_min[i],line_segment_max[i],i,cloud.header.frame_id);
        }
      }
      for(unsigned int i=0; i < inliers.size(); i++)
      {
        double door_frame_width = sqrt ( (line_segment_max[i].x - line_segment_min[i].x) * (line_segment_max[i].x - line_segment_min[i].x) + (line_segment_max[i].y -  line_segment_min[i].y) * (line_segment_max[i].y -  line_segment_min[i].y) );
        if(door_frame_width < door_min_width_ || door_frame_width > door_max_width_)
        {
          inliers[i].resize(0);
          ROS_DEBUG("This candidate line has the wrong width: %f which is outside the (min,max) limits: (%f,%f)",door_frame_width,door_min_width_,door_max_width_);
          continue;
        }
        geometry_msgs::Point32 temp_min,temp_max;
        transform2DInverse(line_segment_min[i],temp_min,global_x_,global_y_,global_yaw_);
        transform2DInverse(line_segment_max[i],temp_max,global_x_,global_y_,global_yaw_);
        double door_pt_angle_1 = atan2(temp_max.y,temp_max.x);
        double door_pt_angle_2 = atan2(temp_min.y,temp_min.x);

        if(fabs(door_pt_angle_1) > M_PI/2.0 || fabs(door_pt_angle_2) > M_PI/2.0)
        {
          inliers[i].resize(0);
          ROS_DEBUG("This candidate line is close to the sensor range edges: may not be a door");
          continue;
        }
      }
      for(unsigned int i=0; i < inliers.size(); i++)
      {
        if((int) inliers[i].size() > inliers_size_max)
        {
          inliers_size_max = (int) inliers[i].size();
          inliers_size_max_index = i;
        }
      }
      if(inliers_size_max_index > -1)
      {
        publishLine(line_segment_min[inliers_size_max_index],line_segment_max[inliers_size_max_index],0, cloud.header.frame_id);
        door_msgs::Door door_tmp;
        door_tmp.door_p1.x = line_segment_min[inliers_size_max_index].x;
        door_tmp.door_p1.y = line_segment_min[inliers_size_max_index].y;
        door_tmp.door_p1.z = line_segment_min[inliers_size_max_index].z;
        door_tmp.door_p2.x = line_segment_max[inliers_size_max_index].x;
        door_tmp.door_p2.y = line_segment_max[inliers_size_max_index].y;
        door_tmp.door_p2.z = line_segment_max[inliers_size_max_index].z;
        door_tmp.header = cloud.header;
        door_tmp.header.frame_id = fixed_frame_;
        door_pub_.publish(door_tmp);
        done_detection_ = true;
        door_result = door_tmp;
        return true;
      }
      ROS_INFO("Done finding door");
      done_detection_ = true;
      return false;
    }

    bool detectDoorService(door_handle_detector::DoorsDetector::Request &req, door_handle_detector::DoorsDetector::Response &resp)
    {
      door_msgs::Door door_tmp;
      door_msg_mutex_.lock();
      door_msg_ = req.door;
      door_msg_mutex_.unlock();

      start();
      ros::Duration tictoc = ros::Duration ().fromSec(0.05);
      while ((int)num_clouds_received_ < 2)// || (cloud_orig_.header.stamp < (start_time + delay)))
      {
        tictoc.sleep ();
      }
      bool res = findDoor(door_tmp);
      shutdown();
      if(res)
      {
        return true;
      }
      else
      {
        return false;
      }
    }

    void activate(const std_msgs::StringConstPtr &activate_msg)
    {
      std_msgs::String msg = *activate_msg;
      ROS_DEBUG("Trying to activate: %s",msg.data.c_str());

      if (msg.data == std::string("activate"))
      {
        if(!continuous_detection_)
        {
          ROS_INFO("Activating continuous detection");
          continuous_detection_ = true;
          start();
        }
      }
      else if (msg.data == std::string("deactivate"))
      {
        if(continuous_detection_)
        {
          ROS_INFO("Deactivating continuous detection");
          continuous_detection_ = false;
          shutdown();
        }
      }
      return;
    }

    void fitSACLines(sensor_msgs::PointCloud *points, vector<int> indices, vector<vector<int> > &inliers, vector<vector<double> > &coeff, vector<geometry_msgs::Point32> &line_segment_min, vector<geometry_msgs::Point32> &line_segment_max)
    {
      geometry_msgs::Point32 minP, maxP;

      // Create and initialize the SAC model
      sample_consensus::SACModelLine *model = new sample_consensus::SACModelLine ();
      sample_consensus::SAC *sac            = new sample_consensus::RANSAC (model, sac_distance_threshold_);
      sac->setMaxIterations (100);
      sac->setProbability (0.95);

      //First find points close to the door frame and neglect points that are far away
      //vector<int> possible_door_points;
      //door_msg_mutex_.lock();
      //obtainCloudIndicesSet(points,possible_door_points,door_msg_,tf_,frame_multiplier_);
      //door_msg_mutex_.unlock();
      //model->setDataSet (points, possible_door_points);
      // Now find the best fit line to this set of points and a corresponding set of inliers
      // Prepare enough space
      // int number_remaining_points = possible_door_points.size();

      vector<int> indices_l;
      indices_l.resize(points->points.size());
      for(unsigned int i=0; i < points->points.size(); i++)      //Use all the indices
      {
        indices_l[i] = i;
      }
      model->setDataSet (points, indices_l);
      int number_remaining_points = indices_l.size();


      while(number_remaining_points > sac_min_points_left_)
      {
        if(sac->computeModel(0))
        {
          if((int) sac->getInliers().size() < sac_min_points_per_model_)
          {
            ROS_DEBUG("Not enough inliers");
            break;
          }
          std::vector<double> new_coeff;
          sac->computeCoefficients(new_coeff);
          sac->refineCoefficients(new_coeff);
          vector<int> inliers_local;
          model->selectWithinDistance(new_coeff, sac_distance_threshold_,inliers_local);

          // Split the inliers into clusters
          vector<vector<int> > clusters;
          findClusters (*points, inliers_local, euclidean_cluster_distance_tolerance_, clusters, -1, 1, 2,
                        euclidean_cluster_angle_tolerance_, euclidean_cluster_min_pts_);
          sort (clusters.begin (), clusters.end (), compareRegions);
          reverse (clusters.begin (), clusters.end ());

          if(clusters.size() == 0)
          {
            number_remaining_points = sac->removeInliers ();
            ROS_DEBUG("Found zero clusters");
            continue;
          }
          ROS_DEBUG("Found %d clusters",(int)clusters.size());
          for(int i=0; i < (int) clusters.size(); i++)
          {
            if((int) clusters[i].size() >  sac_min_points_per_model_)
            {
              coeff.push_back(new_coeff);
              inliers.push_back(clusters[i]);

              //Find the edges of the line segments
              //cloud_geometry::statistics::getMinMax (*points, inliers.back(), minP, maxP);
              cloud_geometry::statistics::getLargestXYPoints (*points, inliers.back(), minP, maxP);
              line_segment_min.push_back(minP);
              line_segment_max.push_back(maxP);
            }
          }
          // Remove the current inliers in the model
          number_remaining_points = sac->removeInliers ();
        }
      }
      delete model;
      delete sac;
    }


    void obtainCloudIndicesSet(sensor_msgs::PointCloud *points, vector<int> &indices, door_msgs::Door door_msg, tf::TransformListener *tf, double frame_multiplier)
    {
      // frames used
      string cloud_frame = points->header.frame_id;
      string door_frame   = door_msg.header.frame_id;

      // Resize the resultant indices to accomodate all data
      indices.resize (points->points.size ());

      // Transform the X-Y bounds from the door request service into the cloud TF frame
      tf::Stamped<geometry_msgs::Point32> frame_p1 (door_msg.frame_p1, points->header.stamp, door_frame);
      tf::Stamped<geometry_msgs::Point32> frame_p2 (door_msg.frame_p2, points->header.stamp, door_frame);
      transformPoint (tf,cloud_frame, frame_p1, frame_p1);
      transformPoint (tf,cloud_frame, frame_p2, frame_p2);

      ROS_DEBUG ("Start detecting door at points in frame %s [%g, %g, %g] -> [%g, %g, %g]",
                 cloud_frame.c_str (), frame_p1.x, frame_p1.y, frame_p1.z, frame_p2.x, frame_p2.y, frame_p2.z);

      // Obtain the bounding box information in the reference frame of the laser scan
      geometry_msgs::Point32 min_bbox, max_bbox;

      if (frame_multiplier == -1)
      {
        ROS_DEBUG ("Door frame multiplier set to -1. Using the entire point cloud data.");
        // Use the complete bounds of the point cloud
        cloud_geometry::statistics::getMinMax (*points, min_bbox, max_bbox);
        for (unsigned int i = 0; i < points->points.size (); i++)
          indices[i] = i;
      }
      else
      {
        // Obtain the actual 3D bounds
        get3DBounds (&frame_p1, &frame_p2, min_bbox, max_bbox, frame_multiplier);

        int nr_p = 0;
        for (unsigned int i = 0; i < points->points.size (); i++)
        {
          if ((points->points[i].x >= min_bbox.x && points->points[i].x <= max_bbox.x) &&
              (points->points[i].y >= min_bbox.y && points->points[i].y <= max_bbox.y) &&
              (points->points[i].z >= min_bbox.z && points->points[i].z <= max_bbox.z))
          {
            indices[nr_p] = i;
            nr_p++;
          }
        }
        indices.resize (nr_p);
      }
      ROS_DEBUG ("Number of points in bounds [%f,%f,%f] -> [%f,%f,%f]: %d.",min_bbox.x, min_bbox.y, min_bbox.z, max_bbox.x, max_bbox.y, max_bbox.z, (int)indices.size ());
    }

    void get3DBounds(geometry_msgs::Point32 *p1, geometry_msgs::Point32 *p2, geometry_msgs::Point32 &min_b, geometry_msgs::Point32 &max_b, int multiplier)
    {
      // Get the door_frame distance in the X-Y plane
      float door_frame = sqrt ( (p1->x - p2->x) * (p1->x - p2->x) + (p1->y - p2->y) * (p1->y - p2->y) );

      float center[2];
      center[0] = (p1->x + p2->x) / 2.0;
      center[1] = (p1->y + p2->y) / 2.0;

      // Obtain the bounds (doesn't matter which is min and which is max at this point)
      min_b.x = center[0] + (multiplier * door_frame) / 2.0;
      min_b.y = center[1] + (multiplier * door_frame) / 2.0;
      min_b.z = -FLT_MAX;

      max_b.x = center[0];
      max_b.y = center[1] - (multiplier * door_frame) / 2.0;
      max_b.z = FLT_MAX;

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
    /** \brief Transform a given point from its current frame to a given target frame
     * \param tf a pointer to a TransformListener object
     * \param target_frame the target frame to transform the point into
     * \param stamped_in the input point
     * \param stamped_out the output point
    */
    inline void transformPoint (tf::TransformListener *tf, const std::string &target_frame,
                    const tf::Stamped< geometry_msgs::Point32 > &stamped_in, tf::Stamped< geometry_msgs::Point32 > &stamped_out)
    {
      tf::Stamped<tf::Point> tmp;
      tmp.stamp_ = stamped_in.stamp_;
      tmp.frame_id_ = stamped_in.frame_id_;
      tmp[0] = stamped_in.x;
      tmp[1] = stamped_in.y;
      tmp[2] = stamped_in.z;

      tf->transformPoint (target_frame, tmp, tmp);

      stamped_out.stamp_ = tmp.stamp_;
      stamped_out.frame_id_ = tmp.frame_id_;
      stamped_out.x = tmp[0];
      stamped_out.y = tmp[1];
      stamped_out.z = tmp[2];
    }

};


int main(int argc, char** argv)
{
  ros::init (argc, argv, "door_tracker");
  DoorTracker p;
  ROS_INFO("Waiting for tracker to finish");
  ros::spin();
  return (0);
}

