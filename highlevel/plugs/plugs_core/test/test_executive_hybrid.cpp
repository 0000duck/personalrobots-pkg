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
 * $Id: test_executive.cpp 14337 2009-04-23 18:40:13Z meeussen $
 *
 *********************************************************************/

/* Author: Melonee Wise */


#include <boost/thread/thread.hpp>

#include <ros/node.h>
#include <robot_actions/action_client.h>

// Msgs
#include <robot_msgs/PlugStow.h>
#include <std_msgs/Empty.h>

// State Msgs
#include <robot_actions/NoArgumentsActionState.h>
#include <pr2_robot_actions/MoveAndGraspPlugState.h>
#include <pr2_robot_actions/DetectOutletState.h>
#include <pr2_robot_actions/StowPlugState.h>
#include <pr2_robot_actions/SwitchControllersState.h>
#include <pr2_robot_actions/DetectPlugOnBaseState.h>
#include <pr2_robot_actions/PlugInState.h>

// Actions
#include <safety_core/action_detect_plug_on_base.h>
#include <safety_core/action_tuck_arms.h>
#include <plugs_core/action_untuck_arms.h>
#include <plugs_core/action_move_and_grasp_plug.h>
#include <plugs_core/action_detect_outlet_fine.h>
#include <plugs_core/action_detect_outlet_coarse.h>
#include <plugs_core/action_localize_plug_in_gripper.h>
#include <plugs_core/action_plug_in.h>
#include <plugs_core/action_stow_plug.h>
#include <plugs_core/action_unplug.h>

using namespace ros;
using namespace std;


// -----------------------------------
//              MAIN
// -----------------------------------

int
  main (int argc, char **argv)
{
  ros::init(argc, argv);

  ros::Node node("test_executive");
  boost::thread_group threads_;



  pr2_robot_actions::SwitchControllers switchlist;
  std_msgs::Empty empty;
  robot_msgs::PlugStow plug_stow;
  robot_msgs::PointStamped point;
  robot_msgs::PoseStamped pose;

  point.header.frame_id = "torso_lift_link";
  point.point.x=0;
  point.point.y=0;
  point.point.z=0;

  Duration timeout_short = Duration().fromSec(3.0);
  Duration timeout_medium = Duration().fromSec(20.0);
  Duration timeout_long = Duration().fromSec(300.0);

  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> tuck_arm("safety_tuck_arms");
  robot_actions::ActionClient<pr2_robot_actions::SwitchControllers, pr2_robot_actions::SwitchControllersState,  std_msgs::Empty> switch_controllers("switch_controllers");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> untuck_arm("plugs_untuck_arms");
  robot_actions::ActionClient<std_msgs::Empty, pr2_robot_actions::DetectPlugOnBaseState, robot_msgs::PlugStow> detect_plug_on_base("detect_plug_on_base");
  robot_actions::ActionClient<robot_msgs::PlugStow, pr2_robot_actions::MoveAndGraspPlugState, std_msgs::Empty> move_and_grasp_plug("move_and_grasp_plug");
  robot_actions::ActionClient<robot_msgs::PointStamped, pr2_robot_actions::DetectOutletState, robot_msgs::PoseStamped> detect_outlet_fine("detect_outlet_fine");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> localize_plug_in_gripper("localize_plug_in_gripper");
  robot_actions::ActionClient<std_msgs::Int32, pr2_robot_actions::PlugInState, std_msgs::Empty>
    plug_in("plug_in");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> unplug("unplug");
  robot_actions::ActionClient< robot_msgs::PlugStow, pr2_robot_actions::StowPlugState, std_msgs::Empty> stow_plug("stow_plug");

  Duration(1.0).sleep();

  tuck_arm.preempt();
  switch_controllers.preempt();
  untuck_arm.preempt();
  detect_plug_on_base.preempt();
  move_and_grasp_plug.preempt();
  detect_outlet_fine.preempt();
  localize_plug_in_gripper.preempt();
  plug_in.preempt();
  unplug.preempt();
  stow_plug.preempt();

  Duration(1.0).sleep();

  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("laser_tilt_controller");
  switchlist.stop_controllers.push_back("r_arm_joint_trajectory_controller");
  switchlist.stop_controllers.push_back("r_arm_hybrid_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_wrench_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_twist_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_trajectory_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS)
    printf("Taking down controllers kinda sorta failed\n");

  Duration(2.0).sleep();

  // detect outlet fine
  if (detect_outlet_fine.execute(point, pose, timeout_long) != robot_actions::SUCCESS) return -3;

  // move and grasp plug
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_joint_trajectory_controller");
  //switchlist.start_controllers.push_back("r_gripper_position_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_trajectory_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_twist_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_wrench_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -2;
  //if (move_and_grasp_plug.execute(plug_stow, empty, timeout_long) != robot_actions::SUCCESS) return -1;

  // localize plug in gripper
  if (localize_plug_in_gripper.execute(empty, empty, timeout_long) != robot_actions::SUCCESS) return -4;

  // plug in
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_cartesian_trajectory_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_wrench_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_twist_controller");
  switchlist.start_controllers.push_back("r_arm_hybrid_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -5;
  std_msgs::Int32 outlet_id; outlet_id.data = 0;
  if (plug_in.execute(outlet_id, empty, timeout_long) != robot_actions::SUCCESS) return -6;

  Duration().fromSec(10.0).sleep();

  //unplug
  if (unplug.execute(empty, empty, timeout_long) != robot_actions::SUCCESS) return -7;

#if 0
  //stow plug
   switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_hybrid_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_trajectory_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_twist_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_wrench_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -8;
  if (stow_plug.execute(plug_stow, empty, timeout_long) != robot_actions::SUCCESS) return -9;


#endif

  timeout_long.sleep();
  timeout_long.sleep();
  timeout_long.sleep();
  timeout_long.sleep();
  timeout_long.sleep();
  // stop remaining controllers
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_hybrid_controller");
  //switchlist.stop_controllers.push_back("laser_tilt_controller");
  //switchlist.stop_controllers.push_back("r_gripper_position_controller");
  //switchlist.stop_controllers.push_back("r_arm_cartesian_trajectory_controller");
  //switchlist.stop_controllers.push_back("r_arm_cartesian_wrench_controller");
  //switchlist.stop_controllers.push_back("r_arm_cartesian_pose_controller");
  //switchlist.stop_controllers.push_back("r_arm_cartesian_twist_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_medium) != robot_actions::SUCCESS) return -1;

  return (0);
}
