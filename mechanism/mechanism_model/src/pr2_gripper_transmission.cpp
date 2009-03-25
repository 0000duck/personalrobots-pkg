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

#include "mechanism_model/pr2_gripper_transmission.h"
#include <algorithm>
#include <numeric>
#include <angles/angles.h>

namespace mechanism {

ROS_REGISTER_TRANSMISSION(PR2GripperTransmission)

bool PR2GripperTransmission::initXml(TiXmlElement *config, Robot *robot)
{
  const char *name = config->Attribute("name");
  name_ = name ? name : "";

  TiXmlElement *ael = config->FirstChildElement("actuator");
  const char *actuator_name = ael ? ael->Attribute("name") : NULL;
  if (!actuator_name || !robot->getActuator(actuator_name))
  {
    ROS_WARN("PR2GripperTransmission could not find actuator named \"%s\"", actuator_name);
    return false;
  }
  robot->getActuator(actuator_name)->command_.enable_ = true;
  actuator_names_.push_back(actuator_name);

  for (TiXmlElement *j = config->FirstChildElement("gap_joint"); j; j = j->NextSiblingElement("gap_joint"))
  {
    const char *joint_name = j->Attribute("name");
    if (!joint_name || !robot->getJoint(joint_name))
    {
      ROS_WARN("PR2GripperTransmission could not find joint named \"%s\"", joint_name);
      return false;
    }
    gap_joint_name_ = std::string("joint_name");
    joint_names_.push_back(joint_name);

    // get the mechanical reduction
    const char *joint_reduction = j->Attribute("mechanical_reduction");
    if (!joint_reduction)
    {
      ROS_WARN("PR2GripperTransmission's joint \"%s\" was not given a reduction.", joint_name);
      return false;
    }
    gap_mechanical_reduction_ = atof(joint_reduction);

    // get the nonlinear transmission parameters
    const char *pA = j->Attribute("A");
    if( pA != NULL )
        A_ = atof(pA);

    const char *pB = j->Attribute("B");
    if( pB != NULL )
        B_ = atof(pB);

    const char *pC = j->Attribute("C");
    if( pC != NULL )
        C_ = atof(pC);
  }

  for (TiXmlElement *j = config->FirstChildElement("passive_joint"); j; j = j->NextSiblingElement("passive_joint"))
  {
    const char *joint_name = j->Attribute("name");
    if (!joint_name || !robot->getJoint(joint_name))
    {
      ROS_WARN("PR2GripperTransmission could not find joint named \"%s\"", joint_name);
      return false;
    }
    joint_names_.push_back(joint_name);
    passive_joints_.push_back(joint_name);

  }

  return true;
}

///////////////////////////////////////////////////////////
/// assign joint position, velocity, effort from actuator state
/// all passive joints are assigned by single actuator state through mimic?
void PR2GripperTransmission::propagatePosition(
  std::vector<Actuator*>& as, std::vector<JointState*>& js)
{
  ROS_ASSERT(as.size() == 1);
  ROS_ASSERT(js.size() == passive_joints_.size() + 1); // passive joints and 1 gap joint

  // rosen: sin(pos*reduction) = A*motor+B
  // rosen: reduction * cos(pos*reduction) * dpos/dt = A * dmotor/dt

  //
  // below transforms from encoder value to gap size, based on 090224_link_data.xls provided by Functions Engineering
  //
  double actuator_angle = as[0]->state_.position_ * gap_mechanical_reduction_; // motor revs
  double arg            = (coef_a_*coef_a_+coef_b_*coef_b_-pow(L0_+actuator_angle*screw_reduction_/gear_ratio_,2))/(2.0*coef_a_*coef_b_);
  double theta          = angles::from_degrees(theta0_ - phi0_) + acos(arg);
  double gap_size_mm    = t0_ + coef_r_ * ( sin(theta) - sin(theta0_) ); // in mm
  double gap_size       = gap_size_mm * mm2m_; // in meters

  //
  // based on similar transforms, get the velocity of the gripper gap size based on encoder velocity
  //
  double actuator_velocity   = as[0]->state_.velocity_ * gap_mechanical_reduction_; // revs per sec
  double arg_dot             = -(L0_ * screw_reduction_)/(gear_ratio_*coef_a_*coef_b_) // d(arg)/d(actuator_angle)
                               -screw_reduction_*actuator_angle*pow(screw_reduction_/gear_ratio_,2);
  double theta_dot           = -1.0/sqrt(1.0-pow(arg,2)) * arg_dot; // derivative of acos
  double gap_velocity        = actuator_velocity * theta_dot;

  //
  // get the effort at the gripper gap based on torque at the motor
  //
  double theta_effort        = as[0]->state_.last_measured_effort_ * gap_mechanical_reduction_;
  double gap_effort          = theta_effort      * theta_dot;


  // assign joint states
  for (unsigned int i = 0; i < js.size(); ++i)
  {
    if (js[i]->joint_->name_ == gap_joint_name_)
    {
      // assign gap joint
      js[i]->position_       = gap_size;
      js[i]->velocity_       = gap_velocity;
      js[i]->applied_effort_ = gap_effort;
        std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " is a gap joint " << std::endl;
    }
    else
    {
      // find the passive joint name
      std::vector<std::string>::iterator it = std::find(passive_joints_.begin(),passive_joints_.end(),js[i]->joint_->name_);
      if (it != passive_joints_.end())
      {
        // assign passive joints
        js[i]->position_       = theta - theta0_ ;
        js[i]->velocity_       = theta_dot      ;
        js[i]->applied_effort_ = theta_effort      ;
        std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " is a passive joint " << std::endl;
      }
      else
      {
        std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " not a gap nor passive joint " << std::endl;
      }
    }
  }
}

// this is needed for simulation, so we can recover encoder value given joint angles
void PR2GripperTransmission::propagatePositionBackwards(
  std::vector<JointState*>& js, std::vector<Actuator*>& as)
{
  ROS_ASSERT(as.size() == 1);
  ROS_ASSERT(js.size() == passive_joints_.size() + 1);

  // obtain the physical location of passive joints in sim, and average them
  double mean_joint_angle  = 0.0;
  double mean_joint_rate   = 0.0;
  double mean_joint_torque = 0.0;
  double count             = 0;

  for (unsigned int i = 0; i < js.size(); ++i)
  {
    // find the passive joint name
    std::vector<std::string>::iterator it = std::find(passive_joints_.begin(),passive_joints_.end(),js[i]->joint_->name_);
    if (it != passive_joints_.end())
    {
      // assign passive joints
      mean_joint_angle    += js[i]->position_      ;
      mean_joint_rate     += js[i]->velocity_      ;
      mean_joint_torque   += js[i]->applied_effort_;
      count++;
      std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " is a passive joint, prop pos backwards " << std::endl;
    }
    else
    {
      std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " not a passive joint " << std::endl;
    }
  }
  
  double avg_joint_angle  = mean_joint_angle  / count;
  double avg_joint_rate   = mean_joint_rate   / count;
  double avg_joint_torque = mean_joint_torque / count;

  // now do the difficult reverse transform
  double theta          = theta0_ + avg_joint_angle; // should we filter this value?
  double arg            = sqrt(2.0*coef_a_*coef_b_*cos(theta-theta0_+phi0_)+coef_h_*coef_h_-coef_a_*coef_a_-coef_b_*coef_b_);
  //double actuator_angle = -gear_ratio_/screw_reduction_ * ( L0_ + arg );   // FIXME: why should line below work and not this one?
  double actuator_angle = -1.0*gear_ratio_/screw_reduction_ * ( L0_ + arg ); 
  double dMR_dtheta     =  gear_ratio_/(2.0 * screw_reduction_) / arg * 2.0 * coef_a_ * coef_b_ * sin(theta + phi0_ - theta0_);

  as[0]->state_.position_             = actuator_angle                / gap_mechanical_reduction_;
  as[0]->state_.velocity_             = avg_joint_rate   * dMR_dtheta / gap_mechanical_reduction_;
  as[0]->state_.last_measured_effort_ = avg_joint_torque * dMR_dtheta / gap_mechanical_reduction_;
}

void PR2GripperTransmission::propagateEffort(
  std::vector<JointState*>& js, std::vector<Actuator*>& as)
{
  ROS_ASSERT(as.size() == 1);
  ROS_ASSERT(js.size() == passive_joints_.size() + 1);

  // obtain the physical location of passive joints in sim, and average them, need this to compute dMR/dtheta
  double mean_joint_angle  = 0.0;
  double count             = 0;

  for (unsigned int i = 0; i < js.size(); ++i)
  {
    // find the passive joint name
    std::vector<std::string>::iterator it = std::find(passive_joints_.begin(),passive_joints_.end(),js[i]->joint_->name_);
    if (it != passive_joints_.end())
    {
      // assign passive joints
      mean_joint_angle    += js[i]->position_            ;
      count++;
      std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " is a passive joint, prop pos backwards " << std::endl;
    }
    else
    {
      std::cout << " js" << js[i]->joint_->name_ << " not a passive joint " << std::endl;
    }
  }
  
  double avg_joint_angle  = mean_joint_angle  / count;

  // now do the difficult reverse transform
  double theta          = theta0_ + avg_joint_angle; // should we filter this value?
  double arg            = sqrt(2.0*coef_a_*coef_b_*cos(theta-theta0_+phi0_)+coef_h_*coef_h_-coef_a_*coef_a_-coef_b_*coef_b_);
  double dMR_dtheta     = gear_ratio_/(2.0 * screw_reduction_) / arg * 2.0 * coef_a_ * coef_b_ * sin(theta + phi0_ - theta0_);


  // get the gap commanded effort
  double gap_commanded_effort = 0.0;
  for (unsigned int i = 0; i < js.size(); ++i)
  {
    if (js[i]->joint_->name_ == gap_joint_name_)
    {
      gap_commanded_effort = js[i]->commanded_effort_ / gap_mechanical_reduction_;
      break; // better be just one of these, need to check
    }
  }

  // assign avctuator commanded effort 
  as[0]->command_.effort_ = gap_commanded_effort * dMR_dtheta / gap_mechanical_reduction_;
}

void PR2GripperTransmission::propagateEffortBackwards(
  std::vector<Actuator*>& as, std::vector<JointState*>& js)
{
  ROS_ASSERT(as.size() == 1);
  ROS_ASSERT(js.size() == passive_joints_.size() + 1);

  //
  // below transforms from encoder value to gap size, based on 090224_link_data.xls provided by Functions Engineering
  //
  double actuator_angle = as[0]->state_.position_ * gap_mechanical_reduction_; // motor revs
  double arg            = (coef_a_*coef_a_+coef_b_*coef_b_-pow(L0_+actuator_angle*screw_reduction_/gear_ratio_,2))/(2.0*coef_a_*coef_b_);
  double theta          = angles::from_degrees(theta0_ - phi0_) + acos(arg);
  double gap_size_mm    = t0_ + coef_r_ * ( sin(theta) - sin(theta0_) ); // in mm
  double gap_size       = gap_size_mm * mm2m_; // in meters

  //
  // based on similar transforms, get the velocity of the gripper gap size based on encoder velocity
  //
  double actuator_velocity   = as[0]->state_.velocity_ * gap_mechanical_reduction_; // revs per sec
  double arg_dot             = -(L0_ * screw_reduction_)/(gear_ratio_*coef_a_*coef_b_) // d(arg)/d(actuator_angle)
                               -screw_reduction_*actuator_angle*pow(screw_reduction_/gear_ratio_,2);
  double theta_dot           = -1.0/sqrt(1.0-pow(arg,2)) * arg_dot; // derivative of acos
  double gap_velocity        = actuator_velocity * theta_dot;

  //
  // get the effort at the gripper gap based on torque at the motor
  //
  double theta_effort        = as[0]->command_.effort_ * gap_mechanical_reduction_;
  double gap_effort          = theta_effort      * theta_dot;


  // assign joint states
  for (unsigned int i = 0; i < js.size(); ++i)
  {
    if (js[i]->joint_->name_ == gap_joint_name_)
    {
      // assign gap joint
      js[i]->commanded_effort_ = gap_effort;
      std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " is a gap joint " << std::endl;
    }
    else
    {
      // find the passive joint name
      std::vector<std::string>::iterator it = std::find(passive_joints_.begin(),passive_joints_.end(),js[i]->joint_->name_);
      if (it != passive_joints_.end())
      {
        // assign passive joints
        js[i]->commanded_effort_ = theta_effort   ;
        std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " is a passive joint " << std::endl;
      }
      else
      {
        std::cout << " js[" << i << "]:" << js[i]->joint_->name_ << " not a gap nor passive joint " << std::endl;
      }
    }
  }

}

} // namespace mechanism
