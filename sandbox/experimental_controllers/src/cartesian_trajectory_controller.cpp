/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Author: Wim Meeussen
 */



#include <algorithm>
#include <mechanism_control/mechanism_control.h>
#include "kdl/chainfksolverpos_recursive.hpp"
#include "experimental_controllers/cartesian_trajectory_controller.h"
#include "pluginlib/class_list_macros.h"


using namespace KDL;
using namespace tf;
using namespace ros;

using namespace controller;

PLUGINLIB_REGISTER_CLASS(CartesianTrajectoryController, CartesianTrajectoryController, Controller)

namespace controller {

ROS_REGISTER_CONTROLLER(CartesianTrajectoryController)

CartesianTrajectoryController::CartesianTrajectoryController()
: jnt_to_pose_solver_(NULL),
  motion_profile_(6, VelocityProfile_Trap(0,0))
{}

CartesianTrajectoryController::~CartesianTrajectoryController()
{}



bool CartesianTrajectoryController::init(mechanism::RobotState *robot_state, const NodeHandle& n)
{
  node_ = n;

  // get name of root and tip from the parameter server
  std::string tip_name;
  if (!node_.getParam("root_name", root_name_)){
    ROS_ERROR("CartesianTrajectoryController: No root name found on parameter server");
    return false;
  }
  if (!node_.getParam("tip_name", tip_name)){
    ROS_ERROR("CartesianTrajectoryController: No tip name found on parameter server");
    return false;
  }

  // test if we got robot pointer
  assert(robot_state);
  robot_state_ = robot_state;

  // create robot chain from root to tip
  if (!chain_.init(robot_state->model_, root_name_, tip_name))
    return false;
  chain_.toKDL(kdl_chain_);

  // create solver
  jnt_to_pose_solver_.reset(new ChainFkSolverPos_recursive(kdl_chain_));
  jnt_pos_.resize(kdl_chain_.getNrOfJoints());

  // initialize motion profile
  double max_vel_trans, max_vel_rot, max_acc_trans, max_acc_rot;
  node_.param("max_vel_trans", max_vel_trans, 0.0) ;
  node_.param("max_vel_rot", max_vel_rot, 0.0) ;
  node_.param("max_acc_trans", max_acc_trans, 0.0) ;
  node_.param("max_acc_rot", max_acc_rot, 0.0) ;
  for (unsigned int i=0; i<3; i++){
    motion_profile_[i  ].SetMax(max_vel_trans, max_acc_trans);
    motion_profile_[i+3].SetMax(max_vel_rot,   max_acc_rot);
  }

  // get a pointer to the pose controller
  std::string output;
  if (!node_.getParam("output", output)){
    ROS_ERROR("CartesianTrajectoryController: No ouptut name found on parameter server");
    return false;
  }
  if (!getController<CartesianPoseController>(output, AFTER_ME, pose_controller_)){
    ROS_ERROR("CartesianTrajectoryController: could not connect to pose controller");
    return false;
  }

  // subscribe to pose commands
  command_notifier_.reset(new MessageNotifier<geometry_msgs::PoseStamped>(tf_,
                                                                       boost::bind(&CartesianTrajectoryController::command, this, _1),
                                                                       node_.getNamespace() + "/command", root_name_, 1));
  // advertise services
  move_to_srv_ = node_.advertiseService("move_to", &CartesianTrajectoryController::moveTo, this);
  preempt_srv_ = node_.advertiseService("preempt", &CartesianTrajectoryController::preempt, this);

  return true;
}




bool CartesianTrajectoryController::moveTo(const geometry_msgs::PoseStamped& pose, const geometry_msgs::Twist& tolerance, double duration)
{
  // don't do anything when still moving
  if (is_moving_) return false;

  // convert message to transform
  Stamped<Pose> pose_stamped;
  poseStampedMsgToTF(pose, pose_stamped);

  // convert to reference frame of root link of the controller chain
  Duration timeout = Duration().fromSec(2.0);
  std::string error_msg;
  if (!tf_.waitForTransform(root_name_, pose.header.frame_id, pose_stamped.stamp_, timeout, Duration(0.01), &error_msg)){
    ROS_ERROR("CartesianTrajectoryController: %s", error_msg.c_str());
    return false;
  }
  tf_.transformPose(root_name_, pose_stamped, pose_stamped);

  // trajectory from pose_begin to pose_end
  TransformToFrame(pose_stamped, pose_end_);
  pose_begin_ = pose_current_;

  max_duration_ = 0;
  Twist twist_move = diff(pose_begin_, pose_end_);

  // Set motion profiles
  for (unsigned int i=0; i<6; i++){
    motion_profile_[i].SetProfileDuration( 0, twist_move(i), duration);
    max_duration_ = max( max_duration_, motion_profile_[i].Duration() );
  }

  // Rescale trajectories to maximal duration
  for (unsigned int i=0; i<6; i++)
    motion_profile_[i].SetProfileDuration( 0, twist_move(i), max_duration_ );

  // set tolerance
  tolerance_.vel(0) = tolerance.linear.x;
  tolerance_.vel(1) = tolerance.linear.y;
  tolerance_.vel(2) = tolerance.linear.z;
  tolerance_.rot(0) = tolerance.angular.x;
  tolerance_.rot(1) = tolerance.angular.y;
  tolerance_.rot(2) = tolerance.angular.z;

  time_passed_ = 0;

  exceed_tolerance_ = false;
  request_preempt_ = false;
  is_moving_ = true;

  ROS_INFO("CartesianTrajectoryController: %s will move to new pose in %f seconds", controller_name_.c_str(), max_duration_);

  return true;
}



bool CartesianTrajectoryController::starting()
{
  // time
  last_time_ = robot_state_->getTime();

  // set desired pose to current pose
  pose_current_ = getPose();
  twist_current_ = Twist::Zero();

  // start not moving
  is_moving_ = false;
  request_preempt_ = false;

  return true;
}




void CartesianTrajectoryController::update()
{
  // get time
  double time = robot_state_->getTime();
  double dt = time - last_time_;
  last_time_ = time;

  // preempt trajectory
  if (request_preempt_){
    twist_current_ = Twist::Zero();
    is_moving_ = false;
  }

  // if we are moving
  if (is_moving_){
    time_passed_ += dt;

    // check tolerance
    for (unsigned int i=0; i<6; i++){
      if (tolerance_[i] != 0 && pose_controller_->twist_error_[i] > tolerance_[i]){
	exceed_tolerance_ = true;
	is_moving_ = false;
      }
    }

    // ended trajectory
    if (time_passed_ > max_duration_){
      twist_current_ = Twist::Zero();
      pose_current_  = pose_end_;
      is_moving_ = false;
    }
    // still in trajectory
    else{
      // pose
      Twist twist_begin_current = Twist(Vector(motion_profile_[0].Pos(time_passed_),
					       motion_profile_[1].Pos(time_passed_),
					       motion_profile_[2].Pos(time_passed_)),
					Vector(motion_profile_[3].Pos(time_passed_),
					       motion_profile_[4].Pos(time_passed_),
					       motion_profile_[5].Pos(time_passed_)) );
      pose_current_ = Frame( pose_begin_.M * Rot( pose_begin_.M.Inverse( twist_begin_current.rot ) ),
			     pose_begin_.p + twist_begin_current.vel);

      // twist
      for(unsigned int i=0; i<6; i++)
	twist_current_(i) = motion_profile_[i].Vel( time_passed_ );
    }
  }

  // send output to pose controller
  pose_controller_->pose_desi_ = pose_current_;
  pose_controller_->twist_ff_ = twist_current_;
}



Frame CartesianTrajectoryController::getPose()
{
  // get the joint positions and velocities
  chain_.getPositions(robot_state_->joint_states_, jnt_pos_);

  // get cartesian pose
  Frame result;
  jnt_to_pose_solver_->JntToCart(jnt_pos_, result);

  return result;
}


bool CartesianTrajectoryController::moveTo(deprecated_srvs::MoveToPose::Request &req,
					   deprecated_srvs::MoveToPose::Response &resp)
{
  ROS_DEBUG("in cartesian traj move_to service");

  if (!moveTo(req.pose, req.tolerance, 0.0)){
    ROS_ERROR("CartesianTrajectoryController: not starting trajectory because either previous one is still running or the transform frame could not be found");
    return false;
  }

  ros::Duration timeout = Duration().fromSec(3.0);
  ros::Duration traj_duration = Duration().fromSec(max_duration_);
  ros::Time start_time = ros::Time::now();

  while (is_moving_){
    Duration().fromSec(0.1).sleep();
    if (ros::Time::now() > start_time + timeout + traj_duration){
      ROS_ERROR("CartesianTrajectoryController: timeout");
      return false;
    }
  }


  if (request_preempt_){
    ROS_ERROR("CartesianTrajectoryController: trajectory preempted");
    return false;
  }
  else if (exceed_tolerance_){
    ROS_ERROR("CartesianTrajectoryController: exceeded trajectory tolerance");
    return false;
  }
  else{
    ROS_DEBUG("CartesianTrajectoryController: moveto finished successfully");
    return true;
  }
}



void CartesianTrajectoryController::command(const MessageNotifier<geometry_msgs::PoseStamped>::MessagePtr& pose_msg)
{
  moveTo(*pose_msg);
}


bool CartesianTrajectoryController::preempt(std_srvs::Empty::Request &req,
					    std_srvs::Empty::Response &resp)
{
  // you can only preempt is the robot is moving
  if (!is_moving_)
    return false;

  request_preempt_ = true;

  // wait for robot to stop moving
  Duration sleep_time = Duration().fromSec(0.01);
  while (is_moving_)
    sleep_time.sleep();

  return true;
}



void CartesianTrajectoryController::TransformToFrame(const Transform& trans, Frame& frame)
{
  frame.p(0) = trans.getOrigin().x();
  frame.p(1) = trans.getOrigin().y();
  frame.p(2) = trans.getOrigin().z();

  double Rz, Ry, Rx;
  trans.getBasis().getEulerZYX(Rz, Ry, Rx);
  frame.M = Rotation::EulerZYX(Rz, Ry, Rx);
}



}; // namespace

