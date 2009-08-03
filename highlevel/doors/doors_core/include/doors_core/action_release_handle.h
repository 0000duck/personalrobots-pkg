/*********************************************************************
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
 *   * Neither the name of Willow Garage nor the names of its
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


/* Author: Wim Meeusen */

#ifndef ACTION_RELEASE_HANDLE_H
#define ACTION_RELEASE_HANDLE_H


#include <ros/node.h>
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <std_srvs/Empty.h>
#include <tf/tf.h>
#include <tf/transform_listener.h>
#include <deprecated_srvs/MoveToPose.h>
#include <kdl/frames.hpp>
#include <robot_actions/action.h>
#include <boost/thread/mutex.hpp>


namespace door_handle_detector{

typedef boost::shared_ptr<geometry_msgs::PoseStamped const> PoseConstPtr;

 class ReleaseHandleAction: public robot_actions::Action<std_msgs::Empty, std_msgs::Empty>
{
public:
  ReleaseHandleAction(tf::TransformListener& tf);
  ~ReleaseHandleAction();

  virtual robot_actions::ResultStatus execute(const std_msgs::Empty& goal, std_msgs::Empty& feedback);

  void poseCallback(const PoseConstPtr& pose);


private:
  ros::NodeHandle node_;
  ros::Publisher gripper_publisher_;

  tf::TransformListener& tf_; 
  tf::Stamped<tf::Pose> gripper_pose_;
  bool pose_received_;
  boost::mutex pose_mutex_;

  std_srvs::Empty::Request  req_empty;
  std_srvs::Empty::Response res_empty;
  deprecated_srvs::MoveToPose::Request  req_moveto;
  deprecated_srvs::MoveToPose::Response res_moveto;
};

}
#endif
