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

#pragma once

/***************************************************/
/*! \class controller::HoldSetController
    \brief Hold Set Controller

    This holds a joint in a set of locations, while
    dithering the joint and recording position and cmd.

*/
/***************************************************/


#include <vector>
#include <ros/node.h>
#include <math.h>
#include <robot_msgs/DiagnosticMessage.h>
#include <joint_qualification_controllers/HoldSetData.h>
#include <realtime_tools/realtime_publisher.h>
#include <realtime_tools/realtime_srv_call.h>
#include <mechanism_model/controller.h>
#include <robot_mechanism_controllers/joint_position_controller.h>
#include <control_toolbox/dither.h>

namespace controller
{

class HoldSetController : public Controller
{

public:
  enum { STARTING, SETTLING, DITHERING, PAUSING, DONE };

  HoldSetController();
  ~HoldSetController();

  /*!
   * \brief Functional way to initialize.
   * \param velocity Target velocity for the velocity controller.
   * \param max_effort Effort to limit the controller at.
   * \param *robot The robot that is being controlled.
   */
  void init( double test_duration, double time, std::string name, mechanism::RobotState *robot);
  bool initXml(mechanism::RobotState *robot, TiXmlElement *config);


  /*!
   * \brief Issues commands to the joint. Should be called at regular intervals
   */

  virtual void update();
  
  bool done() { return state_ == DONE; }
  
  joint_qualification_controllers::HoldSetData::Request hold_set_data_;

private:

  control_toolbox::Dither *dither_;

  controller::JointPositionController *position_controller_;
  std::vector<double> hold_set_;

  mechanism::JointState *joint_state_;      /**< Joint we're controlling. */
  mechanism::RobotState *robot_;            /**< Pointer to robot structure. */


  int starting_count_;

  int state_;
  uint current_position_;

  double initial_time_;

  double settle_time_;
  double start_time_;
  double dither_time_;

  int dither_count_;
  double timeout_;

};

/***************************************************/
/*! \class controller::HoldSetControllerNode
    \brief Hold Set Controller

    This holds a joint at a set of positions, measuring effort

<controller type="HoldSetControllerNode" name="head_tilt_hold_set_controller">
  <joint name="r_shoulder_lift_joint">
    <pid p="15" i="5.0" d="0" iClamp="1.0" />
  </joint>

  <controller_defaults dither_amplitude="0.5" settle_time="2.0" 
  dither_time="1.0" timeout="60" />

  <hold_pt position="1.35" />
  <hold_pt position="1.1" />
  <hold_pt position="0.9" />
  <hold_pt position="0.7" />
  <hold_pt position="0.6" />
  <hold_pt position="0.2" />
  <hold_pt position="-0.2" />
  <hold_pt position="-0.3" />
</controller>

*/
/***************************************************/

class HoldSetControllerNode : public Controller
{
public:

  HoldSetControllerNode();
  ~HoldSetControllerNode();
  
  void update();
  bool initXml(mechanism::RobotState *robot, TiXmlElement *config);

private:
  HoldSetController *c_;
  mechanism::RobotState *robot_;
  
  bool data_sent_;
  
  double last_publish_time_;
  realtime_tools::RealtimeSrvCall<joint_qualification_controllers::HoldSetData::Request, joint_qualification_controllers::HoldSetData::Response> call_service_;
};
}


