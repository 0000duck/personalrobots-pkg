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
 * Author: Stuart Glaser
 */

#include "pr2_mechanism_controllers/caster_calibration_controller.h"

namespace controller {

ROS_REGISTER_CONTROLLER(CasterCalibrationController)

CasterCalibrationController::CasterCalibrationController()
: robot_(NULL), state_(INITIALIZED),
  joint_(NULL), wheel_l_joint_(NULL), wheel_r_joint_(NULL), last_publish_time_(0)
{
}

CasterCalibrationController::~CasterCalibrationController()
{
}

bool CasterCalibrationController::initXml(mechanism::RobotState *robot, TiXmlElement *config)
{
  // This method is gross and ugly and should change (and the xml
  // config format along with it)
  assert(robot);
  assert(config);

  TiXmlElement *cal = config->FirstChildElement("calibrate");
  if (!cal)
  {
    ROS_ERROR("CasterCalibrationController was not given calibration parameters");
    return false;
  }

  if(cal->QueryDoubleAttribute("velocity", &search_velocity_) != TIXML_SUCCESS)
  {
    ROS_ERROR("Velocity value was not specified");
    return false;
  }

  const char *joint_name = cal->Attribute("joint");
  joint_ = joint_name ? robot->getJointState(joint_name) : NULL;
  if (!joint_)
  {
    ROS_ERROR("Error: CasterCalibrationController could not find joint \"%s\"\n",
              joint_name);
    return false;
  }

  const char *actuator_name = cal->Attribute("actuator");
  actuator_ = actuator_name ? robot->model_->getActuator(actuator_name) : NULL;
  if (!actuator_)
  {
    ROS_ERROR("Error: CasterCalibrationController could not find actuator \"%s\"\n",
              actuator_name);
    return false;
  }

  const char *transmission_name = cal->Attribute("transmission");
  transmission_ = transmission_name ? robot->model_->getTransmission(transmission_name) : NULL;
  if (!transmission_)
  {
    ROS_ERROR("Error: CasterCalibrationController could not find transmission \"%s\"\n",
            transmission_name);
    return false;
  }

  if (!cc_.initXml(robot, config))
    return false;

  return true;
}

bool CasterCalibrationController::init(mechanism::RobotState *robot, const ros::NodeHandle &n)
{
  node_ = n;
  robot_ = robot;

  if (!node_.getParam("velocity", search_velocity_))
  {
    ROS_ERROR("No velocity given (namespace: %s)", node_.getNamespace().c_str());
    return false;
  }

  // Joints

  std::string joint_name;
  if (!node_.getParam("joints/caster", joint_name))
  {
    ROS_ERROR("No joint given (namespace: %s)", node_.getNamespace().c_str());
    return false;
  }
  if (!(joint_ = robot->getJointState(joint_name)))
  {
    ROS_ERROR("Could not find joint %s (namespace: %s)",
              joint_name.c_str(), node_.getNamespace().c_str());
    return false;
  }

  if (!node_.getParam("joints/wheel_l", joint_name))
  {
    ROS_ERROR("No joint given (namespace: %s)", node_.getNamespace().c_str());
    return false;
  }
  if (!(wheel_l_joint_ = robot->getJointState(joint_name)))
  {
    ROS_ERROR("Could not find joint %s (namespace: %s)",
              joint_name.c_str(), node_.getNamespace().c_str());
    return false;
  }

  if (!node_.getParam("joints/wheel_r", joint_name))
  {
    ROS_ERROR("No joint given (namespace: %s)", node_.getNamespace().c_str());
    return false;
  }
  if (!(wheel_r_joint_ = robot->getJointState(joint_name)))
  {
    ROS_ERROR("Could not find joint %s (namespace: %s)",
              joint_name.c_str(), node_.getNamespace().c_str());
    return false;
  }

  // Actuator

  std::string actuator_name;
  if (!node_.getParam("actuator", actuator_name))
  {
    ROS_ERROR("No actuator given (namespace: %s)", node_.getNamespace().c_str());
    return false;
  }
  if (!(actuator_ = robot->model_->getActuator(actuator_name)))
  {
    ROS_ERROR("Could not find actuator %s (namespace: %s)",
              actuator_name.c_str(), node_.getNamespace().c_str());
    return false;
  }

  // Transmission

  std::string transmission_name;
  if (!node_.getParam("transmission", transmission_name))
  {
    ROS_ERROR("No transmission given (namespace: %s)", node_.getNamespace().c_str());
    return false;
  }
  if (!(transmission_ = robot->model_->getTransmission(transmission_name)))
  {
    ROS_ERROR("Could not find transmission %s (namespace: %s)",
              transmission_name.c_str(), node_.getNamespace().c_str());
    return false;
  }

  if (!node_.getParam("velocity", search_velocity_))
  {
    ROS_ERROR("Velocity value was not specified (namespace: %s)", node_.getNamespace().c_str());
    return false;
  }

  pub_calibrated_.reset(
    new realtime_tools::RealtimePublisher<std_msgs::Empty>(node_, "calibrated", 1));

  if (!cc_.init(robot_, node_))
    return false;

  return true;
}

void CasterCalibrationController::update()
{
  assert(joint_);
  assert(actuator_);

  switch(state_)
  {
  case INITIALIZED:
    cc_.steer_velocity_ = 0.0;
    cc_.drive_velocity_ = 0.0;
    state_ = BEGINNING;
    break;
  case BEGINNING:
    original_switch_state_ = actuator_->state_.calibration_reading_;
    cc_.steer_velocity_ = (original_switch_state_ ? -search_velocity_ : search_velocity_);
    state_ = MOVING;
    break;
  case MOVING: {
    bool switch_state_ = actuator_->state_.calibration_reading_;
    if (switch_state_ != original_switch_state_)
    {
      Actuator a;
      mechanism::JointState j;
      std::vector<Actuator*> fake_a;
      std::vector<mechanism::JointState*> fake_j;
      fake_a.push_back(&a);
      fake_j.push_back(&j);

      // Where was the joint when the optical switch triggered?
      if (switch_state_ == true)
        fake_a[0]->state_.position_ = actuator_->state_.last_calibration_rising_edge_;
      else
        fake_a[0]->state_.position_ = actuator_->state_.last_calibration_falling_edge_;
      transmission_->propagatePosition(fake_a, fake_j);

      // What is the actuator position at the joint's zero?
      fake_j[0]->position_ = fake_j[0]->position_ - joint_->joint_->reference_position_;
      transmission_->propagatePositionBackwards(fake_j, fake_a);

      actuator_->state_.zero_offset_ = fake_a[0]->state_.position_;
      joint_->calibrated_ = true;
      wheel_l_joint_->calibrated_ = true;
      wheel_r_joint_->calibrated_ = true;

      state_ = CALIBRATED;
    }
    break;
  }
  case CALIBRATED:
    cc_.steer_velocity_ = 0.0;

    if (pub_calibrated_)
    {
      if (last_publish_time_ + 0.5 < robot_->hw_->current_time_)
      {
        if (pub_calibrated_->trylock())
        {
          last_publish_time_ = robot_->hw_->current_time_;
          pub_calibrated_->unlockAndPublish();
        }
      }
    }
    break;
  }

  if (state_ != CALIBRATED)
    cc_.update();
}


ROS_REGISTER_CONTROLLER(CasterCalibrationControllerNode)

CasterCalibrationControllerNode::CasterCalibrationControllerNode()
: last_publish_time_(0), pub_calibrated_(NULL)
{
}

CasterCalibrationControllerNode::~CasterCalibrationControllerNode()
{
  if (pub_calibrated_)
    delete pub_calibrated_;
}

void CasterCalibrationControllerNode::update()
{
  c_.update();

  if (c_.calibrated())
  {
    if (last_publish_time_ + 0.5 < robot_->hw_->current_time_)
    {
      assert(pub_calibrated_);
      if (pub_calibrated_->trylock())
      {
        last_publish_time_ = robot_->hw_->current_time_;
        pub_calibrated_->unlockAndPublish();
      }
    }
  }
}


  bool CasterCalibrationControllerNode::calibrateCommand(robot_mechanism_controllers::CalibrateJoint::Request &req, robot_mechanism_controllers::CalibrateJoint::Response &resp)
{
  c_.beginCalibration();
  ros::Duration d=ros::Duration(0,1000000);
  while(!c_.calibrated())
    d.sleep();
  resp.offset = 0;
  return true;
}

bool CasterCalibrationControllerNode::initXml(mechanism::RobotState *robot, TiXmlElement *config)
{
  assert(robot);
  robot_ = robot;
  ros::Node *node = ros::Node::instance();

  std::string topic = config->Attribute("topic") ? config->Attribute("topic") : "";
  if (topic == "")
  {
    fprintf(stderr, "No topic given to CasterCalibrationController\n");
    return false;
  }

  if (!c_.initXml(robot, config))
    return false;
  node->advertiseService(topic + "/calibrate", &CasterCalibrationControllerNode::calibrateCommand, this);
  guard_calibrate_.set(topic + "/calibrate");

  pub_calibrated_ = new realtime_tools::RealtimePublisher<std_msgs::Empty>(topic + "/calibrated", 1);

  return true;
}


}
