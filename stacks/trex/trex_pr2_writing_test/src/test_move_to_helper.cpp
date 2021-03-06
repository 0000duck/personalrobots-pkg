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
 *
 *
 *********************************************************************/


#include <ros/node.h>
#include <robot_actions/action_client.h>

// Msgs
#include <std_msgs/Empty.h>
#include <pr2_robot_actions/FindHelperResult.h>

// State Msgs
#include <robot_actions/NoArgumentsActionState.h>
#include <pr2_robot_actions/FindHelperState.h>
#include <pr2_robot_actions/SwitchControllersState.h>
#include <nav_robot_actions/MoveBaseState.h>

using namespace ros;
using namespace std;


// -----------------------------------
//              MAIN
// -----------------------------------

int
  main (int argc, char **argv)
{
  ros::init(argc, argv);

  ros::Node node("test_find_helper");

  pr2_robot_actions::SwitchControllers switchlist;
  std_msgs::Empty empty;
 pr2_robot_actions:FindHelperResult find_helper_pose_msg;

  Duration switch_timeout = Duration(4.0);

  robot_actions::ActionClient<pr2_robot_actions::SwitchControllers, pr2_robot_actions::SwitchControllersState,  std_msgs::Empty>
    switch_controllers("switch_controllers");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty>
    tuck_arm("safety_tuck_arms");
  robot_actions::ActionClient<std_msgs::Empty, pr2_robot_actions::FindHelperState, pr2_robot_actions::FindHelperResult> 
    find_helper("find_helper");
  robot_actions::ActionClient<geometry_msgs::PoseStamped, nav_robot_actions::MoveBaseState, geometry_msgs::PoseStamped>
    move_base_local("move_base_local_old");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty>
    start_tilt_laser("set_laser_tilt");
  

  Duration(1.0).sleep();

  // Preempt all of the actions
  switch_controllers.preempt();
  tuck_arm.preempt();
  find_helper.preempt();
  move_base_local.preempt();

  Duration(1.0).sleep();

  // Takes down controllers that might already be up
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_joint_trajectory_controller");
  switchlist.stop_controllers.push_back("head_controller");
  switchlist.stop_controllers.push_back("laser_tilt_controller");
  if (switch_controllers.execute(switchlist, empty, switch_timeout) != robot_actions::SUCCESS) return -1;

  Duration(2.0).sleep();

  // Tuck arms
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.start_controllers.push_back("r_arm_joint_trajectory_controller");
  switchlist.start_controllers.push_back("head_controller");
  switchlist.start_controllers.push_back("laser_tilt_controller");
  if (switch_controllers.execute(switchlist, empty, switch_timeout) != robot_actions::SUCCESS) return -1;
  if (tuck_arm.execute(empty, empty, Duration(20.0)) != robot_actions::SUCCESS) return -2;

  // Find our helper
  if (find_helper.execute(empty, find_helper_pose_msg, Duration(200.0)) != robot_actions::SUCCESS) return -2;

  // Determines the desired base position
  //tf::Pose helper_pose;
  //tf::poseMsgToTF(find_helper_pose_msg.pose, helper_pose);

  //tf::Pose desi_offset(tf::Quaternion(0,0,0), tf::Vector3(-1.5, 0.0, 0.0));
  //tf::Pose target = coarse_outlet_pose * desi_offset;

  //geometry_msgs::PoseStamped target_msg;
  //target_msg.header.frame_id = find_helper_pose_msg.header.frame_id;
  //tf::poseTFToMsg(target, target_msg.pose);

  // Executes move base
  if (start_tilt_laser.execute(empty, empty, Duration(20.0)) != robot_actions::SUCCESS) return -1;
  if (move_base_local.execute(find_helper_pose_msg.zone, find_helper_pose_msg.zone, Duration(500.0)) != robot_actions::SUCCESS) return -4;


  return 0;
}
