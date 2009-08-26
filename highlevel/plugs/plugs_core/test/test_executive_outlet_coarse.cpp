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
#include <tf/tf.h>

#include <ros/node.h>
#include <robot_actions/action_client.h>

// Msgs
#include <plugs_msgs/PlugStow.h>
#include <std_msgs/Empty.h>

// State Msgs
#include <robot_actions/NoArgumentsActionState.h>
#include <pr2_robot_actions/MoveAndGraspPlugState.h>
#include <pr2_robot_actions/DetectOutletState.h>
#include <pr2_robot_actions/StowPlugState.h>
#include <pr2_robot_actions/SwitchControllersState.h>
#include <pr2_robot_actions/DetectPlugOnBaseState.h>
#include <nav_robot_actions/MoveBaseState.h>



using namespace ros;
using namespace std;


// -----------------------------------
//              MAIN
// -----------------------------------


geometry_msgs::Pose transformOutletPose(geometry_msgs::Pose outletPose, float value)
{
    tf::Pose tf_pose;
    geometry_msgs::Pose final_pose;

    tf::poseMsgToTF(outletPose,tf_pose);
    tf::Point point(-value,0,0);
    tf::Point new_origin = tf_pose*point;

    tf_pose.setOrigin(new_origin);

    tf::poseTFToMsg(tf_pose,final_pose);

    return final_pose;
}


int
  main (int argc, char **argv)
{
  ros::init(argc, argv);

  ros::Node node("test_executive");
  boost::thread_group threads_;

  

  pr2_robot_actions::SwitchControllers switchlist;
  std_msgs::Empty empty;
  plugs_msgs::PlugStow plug_stow; 
  geometry_msgs::PointStamped point;
  geometry_msgs::PointStamped fine_outlet_point;
  geometry_msgs::PoseStamped pose;
  
  point.header.frame_id = "odom_combined";
  point.point.x=3.373;
  point.point.y=0.543;
  point.point.z=0;

  Duration timeout_short = Duration().fromSec(2.0);
  Duration timeout_medium = Duration().fromSec(5.0);
  Duration timeout_long = Duration().fromSec(300.0);

  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> tuck_arm("safety_tuck_arms");
  robot_actions::ActionClient<pr2_robot_actions::SwitchControllers, pr2_robot_actions::SwitchControllersState,  std_msgs::Empty> switch_controllers("switch_controllers");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> untuck_arm("plugs_untuck_arms");
  robot_actions::ActionClient<std_msgs::Empty, pr2_robot_actions::DetectPlugOnBaseState, plugs_msgs::PlugStow> detect_plug_on_base("detect_plug_on_base");
  robot_actions::ActionClient<plugs_msgs::PlugStow, pr2_robot_actions::MoveAndGraspPlugState, std_msgs::Empty> move_and_grasp_plug("move_and_grasp_plug");
  robot_actions::ActionClient<geometry_msgs::PointStamped, pr2_robot_actions::DetectOutletState, geometry_msgs::PoseStamped> detect_outlet_fine("detect_outlet_fine");
  robot_actions::ActionClient<geometry_msgs::PointStamped, pr2_robot_actions::DetectOutletState, geometry_msgs::PoseStamped> detect_outlet_coarse("detect_outlet_coarse");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> localize_plug_in_gripper("localize_plug_in_gripper");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> plug_in("plug_in");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> unplug("unplug");
  robot_actions::ActionClient< plugs_msgs::PlugStow, pr2_robot_actions::StowPlugState, std_msgs::Empty> stow_plug("stow_plug");
  robot_actions::ActionClient<geometry_msgs::PoseStamped, nav_robot_actions::MoveBaseState, geometry_msgs::PoseStamped> move_base_local("move_base_local_old");


  timeout_medium.sleep();
  
  // tuck arm
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.start_controllers.push_back("r_arm_joint_trajectory_controller");
  switchlist.start_controllers.push_back("head_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  if (tuck_arm.execute(empty, empty, timeout_long) != robot_actions::SUCCESS) return -1;
  
  printf("Calling Outlet Detect Coarse action\n");

  // detect outlet coarse
  if (detect_outlet_coarse.execute(point, pose, timeout_long) != robot_actions::SUCCESS) return -1;
  
  fine_outlet_point.header = pose.header;
  fine_outlet_point.point = pose.pose.position;

  pose.pose = transformOutletPose(pose.pose, 0.6);

  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.start_controllers.push_back("laser_tilt_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  // move base
  if (move_base_local.execute(pose, pose, timeout_long) != robot_actions::SUCCESS) return -1;
  printf("untuck arm\n");
  // untuck arm
  if (untuck_arm.execute(empty, empty, timeout_medium) != robot_actions::SUCCESS) return -1;

  // detect plug on base
  // if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  if (detect_plug_on_base.execute(empty, plug_stow, timeout_long) != robot_actions::SUCCESS) return -1;

  // move and grasp plug
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_joint_trajectory_controller");
  switchlist.start_controllers.push_back("r_gripper_position_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_trajectory_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_twist_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_wrench_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  if (move_and_grasp_plug.execute(plug_stow, empty, timeout_long) != robot_actions::SUCCESS) return -1;

  // detect outlet fine
  if (detect_outlet_fine.execute(fine_outlet_point, pose, timeout_long) != robot_actions::SUCCESS) return -1;
  
  // localize plug in gripper
  if (localize_plug_in_gripper.execute(empty, empty, timeout_long) != robot_actions::SUCCESS) return -1;

#if 0
  // plug in
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_cartesian_trajectory_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_wrench_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_twist_controller");
  switchlist.start_controllers.push_back("r_arm_hybrid_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  //if (plug_in.execute(empty, empty, timeout_long) != robot_actions::SUCCESS) return -1;
#endif
  timeout_medium.sleep();

  //unplug
  if (unplug.execute(empty, empty, timeout_long) != robot_actions::SUCCESS) return -1;

  //stow plug
  #if 0
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_hybrid_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_trajectory_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_twist_controller");
  switchlist.start_controllers.push_back("r_arm_cartesian_wrench_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  #endif
  if (stow_plug.execute(plug_stow, empty, timeout_long) != robot_actions::SUCCESS) return -1;




  // stop remaining controllers
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("head_tilt_controller");
  switchlist.stop_controllers.push_back("laser_tilt_controller");
  switchlist.stop_controllers.push_back("r_gripper_position_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_trajectory_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_wrench_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_pose_controller");
  switchlist.stop_controllers.push_back("r_arm_cartesian_twist_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_medium) != robot_actions::SUCCESS) return -1;


  timeout_medium.sleep();


  return (0);
}
