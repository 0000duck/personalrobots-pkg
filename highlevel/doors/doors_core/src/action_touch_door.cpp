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

/* Author: Wim Meeussen */

#include <door_functions/door_functions.h>
#include "doors_core/action_touch_door.h"

using namespace tf;
using namespace KDL;
using namespace ros;
using namespace std;
using namespace door_handle_detector;
using namespace door_functions;

static const string fixed_frame = "odom_combined";
static const double touch_dist = 0.65;



TouchDoorAction::TouchDoorAction(tf::TransformListener& tf) : 
  robot_actions::Action<door_msgs::Door, door_msgs::Door>("touch_door"), 
  tf_(tf)
{
  NodeHandle node;
  pub_ = node.advertise<std_msgs::Float64>("r_gripper_effort_controller/command",10);
};


TouchDoorAction::~TouchDoorAction()
{};



robot_actions::ResultStatus TouchDoorAction::execute(const door_msgs::Door& goal, door_msgs::Door& feedback)
{
  ROS_INFO("execute");
 
  // set default feedback
  feedback = goal;
 
  // transform door message to time fixed frame
  door_msgs::Door goal_tr;
  if (!transformTo(tf_, fixed_frame, goal, goal_tr, fixed_frame)){
    ROS_ERROR("Could not tranform door message from '%s' to '%s' at time %f",
	      goal.header.frame_id.c_str(), fixed_frame.c_str(), goal.header.stamp.toSec());
    return robot_actions::ABORTED;
  }

  // check for preemption
  if (isPreemptRequested()) {
    ROS_ERROR("preempted");
    return robot_actions::PREEMPTED;
  }

  // close the gripper while moving to touch the door
  std_msgs::Float64 gripper_msg;
  gripper_msg.data = -20.0;
  pub_.publish(gripper_msg);
  
  // get gripper position that is feasable for the robot arm
  Stamped<Pose> shoulder_pose; tf_.lookupTransform(goal_tr.header.frame_id, "r_shoulder_pan_link", Time(), shoulder_pose);
  // this check does not make sense because we're not tracking the door angle while opinging
  //double angle = getNearestDoorAngle(shoulder_pose, goal_tr, 0.75, touch_dist);
  //if (fabs(angle) > fabs(getDoorAngle(goal_tr))){
  //  ROS_ERROR("Door touch pose for the gripper is inside the door");
  //  return robot_actions::ABORTED;
  //}
  Stamped<Pose> gripper_pose = getGripperPose(goal_tr, getNearestDoorAngle(shoulder_pose, goal_tr, 0.75, touch_dist), touch_dist);
  gripper_pose.stamp_ = Time::now();
  PoseStampedTFToMsg(gripper_pose, req_moveto.pose);
  //req_moveto.tolerance.vel.x = 0.1;
  //req_moveto.tolerance.vel.y = 0.1;
  //req_moveto.tolerance.vel.z = 0.1;

  // move gripper in front of door
  ROS_INFO("move to touch the door at distance %f from hinge", touch_dist);
  if (!ros::service::call("r_arm_constraint_cartesian_trajectory_controller/move_to", req_moveto, res_moveto)){
    if (isPreemptRequested()){
      ROS_ERROR("preempted");
      return robot_actions::PREEMPTED;
    }
    else{
      ROS_ERROR("move_to command failed");
      return robot_actions::ABORTED;
    }
  }

  feedback = goal_tr;
  return robot_actions::SUCCESS;
}


