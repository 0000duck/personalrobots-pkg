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

#include "robot_mechanism_controllers/cartesian_tff_controller.h"
#include <algorithm>
#include <mechanism_control/mechanism_control.h>
#include <kdl/chainfksolvervel_recursive.hpp>


using namespace KDL;
using namespace ros;

static const bool use_constraint_controller = false;


namespace controller {

ROS_REGISTER_CONTROLLER(CartesianTFFController)

CartesianTFFController::CartesianTFFController()
: robot_state_(NULL),
  jnt_to_twist_solver_(NULL),
  mode_(6),
  value_(6),
  twist_to_wrench_(6),
  state_position_publisher_(NULL)
{}



CartesianTFFController::~CartesianTFFController()
{}



bool CartesianTFFController::init(mechanism::RobotState *robot_state, const ros::NodeHandle& n)
{
  node_ = n;

  // get name of root and tip from the parameter server
  std::string root_name, tip_name;
  if (!node_.getParam("root_name", root_name)){
    ROS_ERROR("CartesianTFFController: No root name found on parameter server");
    return false;
  }
  if (!node_.getParam("tip_name", tip_name)){
    ROS_ERROR("CartesianTFFController: No tip name found on parameter server");
    return false;
  }

  // test if we got robot pointer
  assert(robot_state);
  robot_state_ = robot_state;

  // create robot chain from root to tip
  if (!chain_.init(robot_state->model_, root_name, tip_name))
    return false;
  chain_.toKDL(kdl_chain_);

  // create solver
  jnt_to_twist_solver_.reset(new ChainFkSolverVel_recursive(kdl_chain_));
  jnt_posvel_.resize(kdl_chain_.getNrOfJoints());

  // twist to wrench
  double trans, rot;
  node_.param("twist_to_wrench_trans", trans, 0.0);
  for (unsigned int i=0; i<3; i++)
    twist_to_wrench_[i] = trans;
  node_.param("twist_to_wrench_trans", rot, 0.0);
  for (unsigned int i=3; i<6; i++)
    twist_to_wrench_[i] = rot;

  // get pid controllers
  control_toolbox::Pid pid_controller;
  if (!pid_controller.init(NodeHandle(node_, "vel_trans"))) return false;
  for (unsigned int i=0; i<3; i++)
    vel_pid_controller_.push_back(pid_controller);

  if (!pid_controller.init(NodeHandle(node_, "vel_rot"))) return false;
  for (unsigned int i=0; i<3; i++)
    vel_pid_controller_.push_back(pid_controller);

  if (!pid_controller.init(NodeHandle(node_, "pos_trans"))) return false;
  for (unsigned int i=0; i<3; i++)
    pos_pid_controller_.push_back(pid_controller);

  if (!pid_controller.init(NodeHandle(node_, "pos_rot"))) return false;
  for (unsigned int i=0; i<3; i++)
    pos_pid_controller_.push_back(pid_controller);

  // get a pointer to the wrench controller
  std::string output;
  if (!node_.getParam("output", output)){
    ROS_ERROR("CartesianTFFController: No ouptut name found on parameter server");
    return false;
  }
  if (!getController<CartesianWrenchController>(output, AFTER_ME, wrench_controller_)){
    ROS_ERROR("CartesianTFFController: could not connect to wrench controller");
    return false;
  }

  // subscribe to tff commands
  sub_command_ = node_.subscribe<manipulation_msgs::TaskFrameFormalism>("command", 1,
                                 &CartesianTFFController::command, this);

  // realtime publisher for control state
  state_position_publisher_.reset(new realtime_tools::RealtimePublisher<robot_msgs::Twist>(node_, "state/position", 1));

  return true;
}



bool CartesianTFFController::starting()
{
  // time
  last_time_ = robot_state_->hw_->current_time_;

  // set initial modes and values
  for (unsigned int i=0; i<6; i++){
    mode_[i] = manipulation_msgs::TaskFrameFormalism::FORCE;
    value_[i] = 0;
  }

  // reset pid controllers
  for (unsigned int i=0; i<6; i++){
    vel_pid_controller_[i].reset();
    pos_pid_controller_[i].reset();
  }

  // set initial position, twist
  FrameVel frame_twist;
  chain_.getVelocities(robot_state_->joint_states_, jnt_posvel_);
  jnt_to_twist_solver_->JntToCart(jnt_posvel_, frame_twist);
  pose_meas_old_ = frame_twist.value();
  position_ = Twist::Zero();

  loop_count_ = 0;
  return true;
}


void CartesianTFFController::update()
{
  // get time
  double time = robot_state_->hw_->current_time_;
  double dt = time - last_time_;
  last_time_ = time;

  // get the joint positions and velocities
  chain_.getVelocities(robot_state_->joint_states_, jnt_posvel_);

  // get cartesian twist and pose
  FrameVel frame_twist;
  jnt_to_twist_solver_->JntToCart(jnt_posvel_, frame_twist);
  pose_meas_ = frame_twist.value();
  twist_meas_ = pose_meas_.M.Inverse() * (frame_twist.deriv());

  // calculate the distance traveled along the axes of the tf
  position_ += pose_meas_.M.Inverse() * diff(pose_meas_old_, pose_meas_);
  pose_meas_old_ = pose_meas_;

  // calculate desired wrench
  wrench_desi_ = Wrench::Zero();
  for (unsigned int i=0; i<6; i++){
    if (mode_[i] == manipulation_msgs::TaskFrameFormalism::FORCE)
      wrench_desi_[i] = value_[i];
    else if (mode_[i] ==  manipulation_msgs::TaskFrameFormalism::VELOCITY)
      wrench_desi_[i] = twist_to_wrench_[i] * (value_[i] + vel_pid_controller_[i].updatePid(twist_meas_[i] - value_[i], dt));
    else if (mode_[i] == manipulation_msgs::TaskFrameFormalism::POSITION)
      wrench_desi_[i] = twist_to_wrench_[i] * (pos_pid_controller_[i].updatePid(position_[i] - value_[i], dt));
  }

  // send wrench to wrench controller
  wrench_controller_->wrench_desi_ = (pose_meas_.M * wrench_desi_);

  if (++loop_count_ % 100 == 0){
    if (state_position_publisher_){
      if (state_position_publisher_->trylock()){
        state_position_publisher_->msg_.vel.x = position_.vel(0);
        state_position_publisher_->msg_.vel.y = position_.vel(1);
        state_position_publisher_->msg_.vel.z = position_.vel(2);
        state_position_publisher_->msg_.rot.x = position_.rot(0);
	state_position_publisher_->msg_.rot.y = position_.rot(1);
        state_position_publisher_->msg_.rot.z = position_.rot(2);
        state_position_publisher_->unlockAndPublish();
      }
    }
  }

}


void CartesianTFFController::command(const manipulation_msgs::TaskFrameFormalismConstPtr& tff_msg)
{
  mode_[0] = trunc(tff_msg->mode.vel.x);
  mode_[1] = trunc(tff_msg->mode.vel.y);
  mode_[2] = trunc(tff_msg->mode.vel.z);
  mode_[3] = trunc(tff_msg->mode.rot.x);
  mode_[4] = trunc(tff_msg->mode.rot.y);
  mode_[5] = trunc(tff_msg->mode.rot.z);

  value_[0] = tff_msg->value.vel.x;
  value_[1] = tff_msg->value.vel.y;
  value_[2] = tff_msg->value.vel.z;
  value_[3] =  tff_msg->value.rot.x;
  value_[4] =  tff_msg->value.rot.y;
  value_[5] =  tff_msg->value.rot.z;
}



}; // namespace
