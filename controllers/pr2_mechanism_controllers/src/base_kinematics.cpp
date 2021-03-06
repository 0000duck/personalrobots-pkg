/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
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
 *********************************************************************/
/*
 * Author: Sachin Chitta and Matthew Piccoli
 */

#include <pr2_mechanism_controllers/base_kinematics.h>

using namespace controller;


bool Wheel::init(pr2_mechanism::RobotState *robot_state, const ros::NodeHandle &node, std::string link_name)
{
  wheel_stuck_ = 0;
  direction_multiplier_ = 1;
  wheel_speed_filtered_ = 0;
  wheel_speed_error_ = 0;
  wheel_speed_cmd_ = 0;
  wheel_speed_actual_ = 0;

  KDL::SegmentMap::const_iterator link;
  try{
    link = robot_state->model_->robot_model_.getSegment(link_name);
  }
  catch (KDL::Tree::segment_name_exception& ex){
    ROS_ERROR("Could not find link with name %s",link_name.c_str());
    return false;
  }
  ROS_DEBUG("wheel name: %s",link_name.c_str());
  link_name_ = link_name;
  joint_name_ = link->second.segment.getJoint().getName();

  joint_ = robot_state->getJointState(joint_name_);
  KDL::Vector offset = link->second.segment.pose(0.0).p;
  offset_.x = offset.x();
  offset_.y = offset.y();
  offset_.z = offset.z();
  node.param<double> ("wheel_radius_scaler", wheel_radius_scaler_, 1.0);
  ROS_DEBUG("Loading wheel: %s",link_name_.c_str());
  ROS_DEBUG("offset_.x: %f, offset_.y: %f, offset_.z: %f", offset_.x, offset_.y, offset_.z);
  return true;
}

bool Caster::init(pr2_mechanism::RobotState *robot_state,  const ros::NodeHandle &node, std::string link_name)
{
  caster_stuck_ = 0;
  caster_speed_ = 0;
  caster_speed_filtered_ = 0;
  caster_speed_error_ = 0;
  caster_position_error_ = 0;
  steer_angle_stored_ = 0;
  steer_velocity_desired_ = 0;
  steer_angle_actual_ = 0;
  num_children_ = 0;

  KDL::SegmentMap::const_iterator link;
  try{
    link = robot_state->model_->robot_model_.getSegment(link_name);
  }
  catch (KDL::Tree::segment_name_exception& ex){
    ROS_ERROR("Could not find link with name %s",link_name.c_str());
    return false;
  }
  ROS_DEBUG("caster name: %s",link_name.c_str());
  link_name_ = link_name;

  joint_name_ = link->second.segment.getJoint().getName();
  joint_ = robot_state->getJointState(joint_name_);
  KDL::Vector offset = link->second.segment.pose(0.0).p;
  offset_.x = offset.x();
  offset_.y = offset.y();
  offset_.z = offset.z();

  for(unsigned int i=0; i < link->second.children.size(); i++)
  {
    KDL::SegmentMap::const_iterator child = link->second.children[i];
    Wheel tmp;
    parent_->wheel_.push_back(tmp);
    if(!parent_->wheel_[parent_->num_wheels_].init(robot_state, node.getNamespace(), child->second.segment.getName()))
    {
      ROS_ERROR("Could not initialize caster %s",link_name.c_str());
      return false;
    }
    parent_->num_wheels_++;
    num_children_++;
  }
  return true;
}

bool BaseKinematics::init(pr2_mechanism::RobotState *robot_state, const ros::NodeHandle &node)
{
  std::string caster_names_string;
  std::vector<std::string> caster_names;
  name_ = node.getNamespace();
  //Initialize stuff
  MAX_DT_ = 0.01;
  num_wheels_ = 0;
  num_casters_ = 0;

  robot_state_ = robot_state;

  node.param<std::string> ("caster_names",caster_names_string,"");
  std::stringstream ss(caster_names_string);
  std::string tmp;
  while(ss >> tmp)
  {
    caster_names.push_back(tmp);
  }



  for(unsigned int i=0; i < caster_names.size(); i++)
  {
    Caster tmp;
    caster_.push_back(tmp);
    caster_[num_casters_].parent_ = this;
    ROS_DEBUG("caster name: %s",caster_names[i].c_str());
    if(!caster_[num_casters_].init(robot_state, name_, caster_names[i]))
    {
      ROS_ERROR("Could not initialize base kinematics");
      return false;
    }
    num_casters_++;
  }
  int wheel_counter = 0;
  for(int j = 0; j < num_casters_; j++)
  {
    for(int i = 0; i < caster_[j].num_children_; i++)
    {
      wheel_[wheel_counter].parent_ = &(caster_[j]);
      wheel_counter++;
    }
  }

  node.param<double> ("wheel_radius", wheel_radius_, 0.074792);
  double multiplier;
  node.param<double> ("wheel_radius_multiplier", multiplier, 1.0);
  wheel_radius_ = wheel_radius_ * multiplier;

  return true;
}

void Wheel::updatePosition()
{
  geometry_msgs::Point result = parent_->offset_;
  double costh = cos(parent_->joint_->position_);
  double sinth = sin(parent_->joint_->position_);
  result.x += costh * offset_.x - sinth * offset_.y;
  result.y += sinth * offset_.x + costh * offset_.y;
  result.z = 0.0;
  position_ = result;
}

void BaseKinematics::computeWheelPositions()
{
  for(int i = 0; i < num_wheels_; i++)
  {
    wheel_[i].updatePosition();
  }

}

geometry_msgs::Twist BaseKinematics::pointVel2D(const geometry_msgs::Point& pos, const geometry_msgs::Twist& vel)
{
  geometry_msgs::Twist result;
  result.linear.x = vel.linear.x - pos.y * vel.angular.z;
  result.linear.y = vel.linear.y + pos.x * vel.angular.z;
  result.angular.z = 0;
  return result;
}
