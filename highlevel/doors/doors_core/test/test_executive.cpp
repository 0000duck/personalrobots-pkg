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
 * $Id$
 *
 *********************************************************************/

/* Author: Wim Meeussen */


#include <boost/thread/thread.hpp>
#include <door_msgs/Door.h>
#include <ros/node.h>
#include <robot_actions/action_client.h>
#include <pr2_robot_actions/Pose2D.h>
#include <pr2_robot_actions/DoorActionState.h>
#include <robot_actions/NoArgumentsActionState.h>
#include <pr2_robot_actions/SwitchControllersState.h>
#include <nav_robot_actions/MoveBaseState.h>
#include <door_functions/door_functions.h>


using namespace ros;
using namespace std;
using namespace door_functions;




// -----------------------------------
//              MAIN
// -----------------------------------

int
  main (int argc, char **argv)
{
  ros::init(argc, argv);

  ros::Node node("test_executive");
  boost::thread* thread;

  door_msgs::Door prior_door;
  prior_door.frame_p1.x = 1.0;
  prior_door.frame_p1.y = -0.5;
  prior_door.frame_p2.x = 1.0;
  prior_door.frame_p2.y = 0.5;
  prior_door.door_p1.x = 1.0;
  prior_door.door_p1.y = -0.5;
  prior_door.door_p2.x = 1.0;
  prior_door.door_p2.y = 0.5;
  prior_door.travel_dir.x = 1.0;
  prior_door.travel_dir.y = 0.0;
  prior_door.travel_dir.z = 0.0;
  prior_door.rot_dir = door_msgs::Door::ROT_DIR_COUNTERCLOCKWISE;
  //prior_door.rot_dir = door_msgs::Door::ROT_DIR_CLOCKWISE;
  prior_door.hinge = door_msgs::Door::HINGE_P2;
  prior_door.header.frame_id = "base_footprint";
  /*
  prior_door.frame_p1.x = 13.720980644226074;
  prior_door.frame_p1.y = 21.296588897705078;
  prior_door.frame_p2.x = 13.784474372863770;
  prior_door.frame_p2.y = 22.195737838745117;
  prior_door.door_p1.x = 13.720980644226074;
  prior_door.door_p1.y = 21.296588897705078;
  prior_door.door_p2.x = 13.784474372863770;
  prior_door.door_p2.y = 22.195737838745117;
  prior_door.travel_dir.x = -1.936123440473645;
  prior_door.travel_dir.y = 3.251793805466352;
  prior_door.rot_dir = door_msgs::Door::ROT_DIR_COUNTERCLOCKWISE;
  prior_door.hinge = door_msgs::Door::HINGE_P1;
  prior_door.header.frame_id = "map";
  */    
  pr2_robot_actions::SwitchControllers switchlist;
  std_msgs::Empty empty;

  Duration timeout_short = Duration().fromSec(2.0);
  Duration timeout_medium = Duration().fromSec(10.0);
  Duration timeout_long = Duration().fromSec(40.0);

  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> tuck_arm("safety_tuck_arms");
  robot_actions::ActionClient<pr2_robot_actions::SwitchControllers, pr2_robot_actions::SwitchControllersState,  std_msgs::Empty> switch_controllers("switch_controllers");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> detect_door("detect_door");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> detect_handle("detect_handle");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> grasp_handle("grasp_handle");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> touch_door("touch_door");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> unlatch_handle("unlatch_handle");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> open_door("open_door");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> push_door("push_door");
  robot_actions::ActionClient<std_msgs::Empty, robot_actions::NoArgumentsActionState, std_msgs::Empty> release_handle("release_handle");
  robot_actions::ActionClient<door_msgs::Door, pr2_robot_actions::DoorActionState, door_msgs::Door> move_base_door("move_base_door");
  robot_actions::ActionClient<robot_msgs::PoseStamped, nav_robot_actions::MoveBaseState, robot_msgs::PoseStamped> move_base_local("move_base_local");

  door_msgs::Door tmp_door;
  door_msgs::Door backup_door;

  cout << "before " << prior_door << endl;

  // tuck arm
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.start_controllers.push_back("r_arm_joint_trajectory_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_medium) != robot_actions::SUCCESS) return -1;
  if (tuck_arm.execute(empty, empty, timeout_medium) != robot_actions::SUCCESS) return -1;
  cout << "tuck arms " << endl;

  // detect door
  door_msgs::Door res_detect_door;
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.start_controllers.push_back("laser_tilt_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  while (detect_door.execute(prior_door, tmp_door, timeout_long) != robot_actions::SUCCESS);
  res_detect_door = tmp_door;
  cout << "detect door " << res_detect_door << endl;

  // detect handle if door is latched
  door_msgs::Door res_detect_handle;
  bool open_by_pushing = false;
  if (res_detect_door.latch_state == door_msgs::Door::UNLATCHED)
    open_by_pushing = true;
  if (!open_by_pushing){
    switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
    switchlist.start_controllers.push_back("head_controller");
    switchlist.start_controllers.push_back("head_pan_joint_position_controller");
    switchlist.start_controllers.push_back("head_tilt_joint_position_controller");
    if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
    while (detect_handle.execute(res_detect_door, tmp_door, timeout_long) != robot_actions::SUCCESS);
    res_detect_handle = tmp_door;
    cout << "detect handle " << res_detect_handle << endl;
  }

  // approach door
  robot_msgs::PoseStamped goal_msg;
  tf::poseStampedTFToMsg(getRobotPose(res_detect_door, -0.6), goal_msg);
  cout << "move to pose " << goal_msg.pose.position.x << ", " << goal_msg.pose.position.y << ", "<< goal_msg.pose.position.z << endl;
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  while (move_base_local.execute(goal_msg, goal_msg) != robot_actions::SUCCESS) {cout << "re-trying move base local" << endl;};
  cout << "door approach finished" << endl;

  // touch door
  if (open_by_pushing){
    switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
    switchlist.stop_controllers.push_back("r_arm_joint_trajectory_controller");
    switchlist.start_controllers.push_back("r_gripper_effort_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_trajectory_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_pose_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_twist_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_wrench_controller");
    if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
    if (touch_door.execute(res_detect_door, tmp_door, timeout_long) != robot_actions::SUCCESS) return -1;
    cout << "door touched" << endl;

    // push door in separate thread
    switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
    switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_trajectory_controller");
    if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
    thread = new boost::thread(boost::bind(&robot_actions::ActionClient<door_msgs::Door, 
					   pr2_robot_actions::DoorActionState, door_msgs::Door>::execute, 
					   &push_door, res_detect_door, tmp_door, timeout_long));
  }
  else{
    // grasp handle
    switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
    switchlist.stop_controllers.push_back("r_arm_joint_trajectory_controller");
    switchlist.start_controllers.push_back("r_gripper_effort_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_trajectory_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_pose_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_twist_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_wrench_controller");
    if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
    if (grasp_handle.execute(res_detect_handle, tmp_door, timeout_long) != robot_actions::SUCCESS) return -1;

    // unlatch handle
    switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
    switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_trajectory_controller");
    switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_pose_controller");
    switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_twist_controller");
    switchlist.start_controllers.push_back("r_arm_cartesian_tff_controller");
    if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
    if (unlatch_handle.execute(res_detect_handle, tmp_door, timeout_long) != robot_actions::SUCCESS) return -1;

    // open door in separate thread
    switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
    if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
    thread = new boost::thread(boost::bind(&robot_actions::ActionClient<door_msgs::Door, 
					   pr2_robot_actions::DoorActionState, door_msgs::Door>::execute, 
					   &open_door, res_detect_handle, tmp_door, timeout_long));
  }    

  bool move_thru_success = true;
  // move throught door
  pr2_robot_actions::Pose2D pose2d;
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
  cout << "moving through door with door message " << res_detect_handle << endl;
  if (move_base_door.execute(res_detect_door, tmp_door) != robot_actions::SUCCESS) 
    {
      move_thru_success = false;
      backup_door = res_detect_door;
    };


  // preempt open/push door
  if (open_by_pushing)
    push_door.preempt();
  else
    open_door.preempt();
  thread->join();
  delete thread;


  // release handle
  if (!open_by_pushing){
    switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
    switchlist.stop_controllers.push_back("r_arm_cartesian_tff_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_trajectory_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_pose_controller");
    switchlist.start_controllers.push_back("r_arm_constraint_cartesian_twist_controller");
    if (switch_controllers.execute(switchlist, empty, timeout_short) != robot_actions::SUCCESS) return -1;
    if (release_handle.execute(empty, empty, timeout_long) != robot_actions::SUCCESS) return -1;
  }

  // tuck arm
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_trajectory_controller");
  switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_pose_controller");
  switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_twist_controller");
  switchlist.stop_controllers.push_back("r_arm_constraint_cartesian_wrench_controller");
  switchlist.start_controllers.push_back("r_arm_joint_trajectory_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_medium) != robot_actions::SUCCESS) return -1;
  if (tuck_arm.execute(empty, empty, timeout_medium) != robot_actions::SUCCESS) return -1;

  if(!move_thru_success)
  {
    cout  << "using door for backup" << endl << backup_door << endl;
    backup_door.travel_dir.x = -1.0;
    backup_door.travel_dir.y = 0.0;
    if (move_base_door.execute(backup_door, tmp_door) != robot_actions::SUCCESS)
      return -1;
  }

  // stop remaining controllers
  switchlist.start_controllers.clear();  switchlist.stop_controllers.clear();
  switchlist.stop_controllers.push_back("laser_tilt_controller");
  switchlist.stop_controllers.push_back("head_controller");
  switchlist.stop_controllers.push_back("r_gripper_effort_controller");
  switchlist.stop_controllers.push_back("r_arm_joint_trajectory_controller");
  if (switch_controllers.execute(switchlist, empty, timeout_medium) != robot_actions::SUCCESS) return -1;

  return (0);
}
