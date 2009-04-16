/*********************************************************************
*
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
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
*
* Author: Eitan Marder-Eppstein
*********************************************************************/
#ifndef TRAJECTORY_ROLLOUT_TRAJECTORY_PLANNER_ROS_H_
#define TRAJECTORY_ROLLOUT_TRAJECTORY_PLANNER_ROS_H_

#include <ros/node.h>
#include <costmap_2d/costmap_2d.h>
#include <base_local_planner/world_model.h>
#include <base_local_planner/point_grid.h>
#include <base_local_planner/costmap_model.h>
#include <base_local_planner/voxel_grid_model.h>
#include <base_local_planner/trajectory_planner.h>

#include <base_local_planner/planar_laser_scan.h>

#include <tf/transform_datatypes.h>

#include <deprecated_msgs/RobotBase2DOdom.h>
#include <robot_actions/Pose2D.h>
#include <robot_msgs/PoseDot.h>
#include <robot_msgs/Point.h>

#include <laser_scan/LaserScan.h>
#include <tf/message_notifier.h>
#include <tf/transform_listener.h>
#include <laser_scan/laser_scan.h>

#include <boost/thread.hpp>

#include <string>

namespace base_local_planner {
  /**
   * @class TrajectoryPlannerROS
   * @brief A ROS wrapper for the trajectory controller that queries the param server to construct a controller
   */
  class TrajectoryPlannerROS{
    public:
      /**
       * @brief  Constructs the ros wrapper
       * @param ros_node The node that is running the controller, used to get parameters from the parameter server
       * @param tf A reference to a transform listener
       * @param cost_map The cost map to use for assigning costs to trajectories
       * @param footprint_spec A polygon representing the footprint of the robot. (Must be convex)
       * @param planner_map Used to size the map for the freespace controller... this will go away once a rolling window version of the point grid is in place
       */
      TrajectoryPlannerROS(ros::Node& ros_node, tf::TransformListener& tf,
          const costmap_2d::Costmap2D& cost_map, std::vector<robot_msgs::Point> footprint_spec,
          const costmap_2d::Costmap2D* planner_map = NULL);

      /**
       * @brief  Destructor for the wrapper
       */
      ~TrajectoryPlannerROS();
      
      /**
       * @brief  Used for display purposes, allows the footprint of the robot to be drawn in visualization tools
       * @param x_i The x position of the robot
       * @param y_i The y position of the robot
       * @param theta_i The orientation of the robot
       * @return A vector of points in world coordinates that correspond to the verticies of the robot's footprint 
       */
      std::vector<robot_msgs::Point> drawFootprint(double x_i, double y_i, double theta_i);

      /**
       * @brief  Given the current position, orientation, and velocity of the robot, compute velocity commands to send to the base
       * @param global_plan The plan to pass to the controller
       * @param cmd_vel Will be filled with the velocity command to be passed to the robot base
       * @param local_plan Will be set from the points of the selected trajectory for display purposes
       * @param observations A vector of updates from the robot's sensors in world space, is sometimes unused depending on the model
       * @return True if a valid trajectory was found, false otherwise
       */
      bool computeVelocityCommands(const std::vector<robot_actions::Pose2D>& global_plan, 
          robot_msgs::PoseDot& cmd_vel,
          std::vector<robot_actions::Pose2D>& local_plan,
          const std::vector<costmap_2d::Observation>& observations = std::vector<costmap_2d::Observation>(0));

      /**
       * @brief  Returns the local goal the robot is pursuing
       * @param x Will be set to the x position of the goal in world coordinates 
       * @param y Will be set to the y position of the goal in world coordinates 
       * @return 
       */
      void getLocalGoal(double& x, double& y);

      /**
       * @brief  Check whether the robot is stopped or not
       * @return True if the robot is stopped, false otherwise
       */
      bool stopped();

      void baseScanCallback(const tf::MessageNotifier<laser_scan::LaserScan>::MessagePtr& message);
      void tiltScanCallback(const tf::MessageNotifier<laser_scan::LaserScan>::MessagePtr& message);
      void odomCallback();

    private:
      WorldModel* world_model_; ///< @brief The world model that the controller will use
      TrajectoryPlanner* tc_; ///< @brief The trajectory controller
      tf::MessageNotifier<laser_scan::LaserScan>* base_scan_notifier_; ///< @brief Used to guarantee that a transform is available for base scans
      tf::MessageNotifier<laser_scan::LaserScan>* tilt_scan_notifier_; ///< @brief Used to guarantee that a transform is available for tilt scans
      tf::TransformListener& tf_; ///< @brief Used for transforming point clouds
      std::string global_frame_; ///< @brief The frame in which the controller will run
      laser_scan::LaserProjection projector_; ///< @brief Used to project laser scans into point clouds
      boost::recursive_mutex obs_lock_; ///< @brief Lock for accessing data in callbacks safely
      std::vector<PlanarLaserScan> laser_scans_; ///< @breif Storage for the last scan the lasers took... used for clearing free-space in front of the robot
      PointGrid* point_grid_; ///< @brief If using a freespace grid... we want to access it
      VoxelGridModel* voxel_grid_; ///< @brief If using a voxel grid... we want to access it
      double max_sensor_range_; ///< @brief Keep track of the effective maximum range of our sensors
      deprecated_msgs::RobotBase2DOdom odom_msg_, base_odom_; ///< @brief Used to get the velocity of the robot
      std::string robot_base_frame_; ///< @brief Used as the base frame id of the robot
      double rot_stopped_velocity_, trans_stopped_velocity_;
  };

};

#endif
