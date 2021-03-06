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


#include "pr2_mechanism_controllers/head_position_controller.h"
#include <cmath>
#include "pluginlib/class_list_macros.h"

using namespace tf;
using namespace std;

PLUGINLIB_REGISTER_CLASS(HeadPositionController, controller::HeadPositionController, controller::Controller)

namespace controller {

HeadPositionController::HeadPositionController()
  : robot_state_(NULL),
    point_head_notifier_(NULL),
    point_frame_on_head_notifier_(NULL)
{}

HeadPositionController::~HeadPositionController()
{
  sub_command_.shutdown();

}

bool HeadPositionController::init(pr2_mechanism::RobotState *robot_state, const ros::NodeHandle &n)
{
  node_ = n;

// get name link names from the param server
  if (!node_.getParam("pan_link_name", pan_link_name_)){
    ROS_ERROR("HeadPositionController: No pan link name found on parameter server (namespace: %s)",
              node_.getNamespace().c_str());
    return false;
  }
  if (!node_.getParam("tilt_link_name", tilt_link_name_)){
    ROS_ERROR("HeadPositionController: No tilt link name found on parameter server (namespace: %s)",
              node_.getNamespace().c_str());
    return false;
  }

  // test if we got robot pointer
  assert(robot_state);
  robot_state_ = robot_state;

  //initialize the joint position controllers

  head_pan_controller_.init(robot_state, ros::NodeHandle(node_, "pan_controller"));
  head_tilt_controller_.init(robot_state, ros::NodeHandle(node_, "tilt_controller"));


  // subscribe to head commands
  sub_command_ = node_.subscribe<sensor_msgs::JointState>("command", 1, &HeadPositionController::command, this);

  point_head_notifier_.reset(new MessageNotifier<geometry_msgs::PointStamped>(tf_,
                                                                       boost::bind(&HeadPositionController::pointHead, this, _1),
                                                                       node_.getNamespace() + "/point_head", pan_link_name_, 10));
  point_frame_on_head_notifier_.reset(new MessageNotifier<geometry_msgs::PointStamped>(tf_,
                                                                       boost::bind(&HeadPositionController::pointFrameOnHead, this, _1),
                                                                       node_.getNamespace() + "/point_frame_on_head", pan_link_name_, 10));

  return true;
}

bool HeadPositionController::starting()
{
  pan_out_ = head_pan_controller_.joint_state_->position_;
  tilt_out_ = head_tilt_controller_.joint_state_->position_;
  head_pan_controller_.starting();
  head_tilt_controller_.starting();
  return true;
}

void HeadPositionController::update()
{
  // set position controller commands
  head_pan_controller_.command_ = pan_out_;
  head_tilt_controller_.command_ = tilt_out_;
  head_pan_controller_.update();
  head_tilt_controller_.update();
}

void HeadPositionController::command(const sensor_msgs::JointStateConstPtr& command_msg)
{
  // do not use assert to check user input!
 
  if (command_msg->name.size() != 2 || command_msg->position.size() != 2){
    ROS_ERROR("Head servoing controller expected joint command of size 2");
    return;
  }
  if (command_msg->name[0] == head_pan_controller_.joint_state_->joint_->name_ &&
      command_msg->name[1] == head_tilt_controller_.joint_state_->joint_->name_)
  {
    pan_out_  = command_msg->position[0];
    tilt_out_ = command_msg->position[1];
  }
  else if (command_msg->name[1] == head_pan_controller_.joint_state_->joint_->name_ &&
           command_msg->name[0] == head_tilt_controller_.joint_state_->joint_->name_)
  {
    pan_out_ = command_msg->position[1];
    tilt_out_ = command_msg->position[0];
  }
  else
  {
    ROS_ERROR("Head servoing controller received invalid joint command");
  }
}

void HeadPositionController::pointHead(const tf::MessageNotifier<geometry_msgs::PointStamped>::MessagePtr& point_msg)
{
  std::string pan_parent;
  tf_.getParent( pan_link_name_, ros::Time(), pan_parent);
  // convert message to transform
  Stamped<Point> pan_point;
  pointStampedMsgToTF(*point_msg, pan_point);

  // convert to reference frame of pan link parent
  tf_.transformPoint(pan_parent, pan_point, pan_point);
  //calculate the pan angle
  pan_out_ = atan2(pan_point.y(), pan_point.x());

  Stamped<Point> tilt_point;
  pointStampedMsgToTF(*point_msg, tilt_point);
  // convert to reference frame of pan link
  tf_.transformPoint(pan_link_name_, tilt_point, tilt_point);
  //calculate the tilt angle
  tilt_out_ = atan2(-tilt_point.z(), sqrt(tilt_point.x()*tilt_point.x() + tilt_point.y()*tilt_point.y()));


}

void HeadPositionController::pointFrameOnHead(const tf::MessageNotifier<geometry_msgs::PointStamped>::MessagePtr& point_msg)
{

  Stamped<tf::Transform> frame;
  try
  {
    tf_.lookupTransform(point_msg->header.frame_id, pan_link_name_, ros::Time(), frame);
  }
  catch(TransformException& ex)
  {
    ROS_WARN("Transform Exception %s", ex.what());
    return;
  }

  // convert message to transform
  Stamped<Point> pan_point;
  pointStampedMsgToTF(*point_msg, pan_point);
  tf_.transformPoint(pan_link_name_, pan_point, pan_point);

  double radius = pow(pan_point.x(),2)+pow(pan_point.y(),2);
  double x_intercept = sqrt(fabs(radius-pow(frame.getOrigin().y(),2)));
  double delta_theta = atan2(pan_point.y(), pan_point.x())-atan2(frame.getOrigin().y(),x_intercept);
  pan_out_ = head_pan_controller_.joint_state_->position_ + delta_theta;

  // convert message to transform
  Stamped<Point> tilt_point;
  pointStampedMsgToTF(*point_msg, tilt_point);
  tf_.transformPoint(pan_link_name_, tilt_point, tilt_point);

  radius = pow(tilt_point.x(),2)+pow(tilt_point.y(),2);
  x_intercept = sqrt(fabs(radius-pow(frame.getOrigin().z(),2)));
  delta_theta = atan2(-tilt_point.z(), sqrt(tilt_point.x()*tilt_point.x() + tilt_point.y()*tilt_point.y()))-atan2(frame.getOrigin().z(),x_intercept);
  tilt_out_ = delta_theta;

}

}//namespace

