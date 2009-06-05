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
/*! \class controller::BacklashController
    \brief Backlash Controller

    This class attempts to find backlash in joint.
*/
/***************************************************/


#include <ros/node.h>
#include <math.h>
#include <diagnostic_msgs/DiagnosticMessage.h>
#include <realtime_tools/realtime_publisher.h>
#include <realtime_tools/realtime_srv_call.h>
#include <mechanism_model/controller.h>
#include <control_toolbox/sine_sweep.h>
#include <robot_mechanism_controllers/joint_velocity_controller.h>
#include <joint_qualification_controllers/TestData.h>

namespace controller
{

class BacklashController : public Controller
{
public:
  /*!
   * \brief Default Constructor of the BacklashController class.
   *
   */
  BacklashController();

  /*!
   * \brief Destructor of the BacklashController class.
   */
  ~BacklashController();

  /*!
   * \brief Functional way to initialize.
   * \param freq Freq of backlash test (Hz).
   * \param amplitude The amplitude of the sweep (N).
   * \param duration The duration in seconds from start to finish (s).
   * \param error_tolerance Maximum error permitted
   * \param time The current hardware time.
   * \param *robot The robot that is being controlled.
   */
  void init(double freq, double duration, double amplitude, double error_tolerance, double time, std::string name,mechanism::RobotState *robot);
  bool initXml(mechanism::RobotState *robot, TiXmlElement *config);

  void analysis();
  virtual void update();

  inline bool done() { return done_; }

  diagnostic_msgs::DiagnosticMessage diagnostic_message_;
  joint_qualification_controllers::TestData::Request test_data_;

private:
  mechanism::JointState *joint_state_;      /**< Joint we're controlling. */
  mechanism::RobotState *robot_;            /**< Pointer to robot structure. */
  double duration_;                         /**< Duration of the test. */
  double initial_time_;                     /**< Start time of the test. */
  int count_;
  bool done_;
  double last_time_;
  double amplitude_;
  double freq_;

};

/***************************************************/
/*! \class controller::BacklashControllerNode
    \brief Backlash Controller ROS Node

*/
/***************************************************/

class BacklashControllerNode : public Controller
{
public:
 
  BacklashControllerNode();
  ~BacklashControllerNode();

  void update();
  bool initXml(mechanism::RobotState *robot, TiXmlElement *config);

private:
  bool data_sent_;
  BacklashController *c_;
  mechanism::RobotState *robot_;
  
  double last_publish_time_;
  realtime_tools::RealtimeSrvCall<joint_qualification_controllers::TestData::Request, joint_qualification_controllers::TestData::Response> call_service_;
  realtime_tools::RealtimePublisher<diagnostic_msgs::DiagnosticMessage> pub_diagnostics_;
};
}


