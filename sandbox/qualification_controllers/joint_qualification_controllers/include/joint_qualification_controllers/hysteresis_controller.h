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
/*! \class controller::HysteresisController
    \brief Hystersis Controller

    This tests the hysteresis of a joint using a
    velocity controller.

*/
/***************************************************/

#include <ros/ros.h>
#include <string>
#include <math.h>
#include <joint_qualification_controllers/TestData.h>
#include <realtime_tools/realtime_srv_call.h>
#include <controller_interface/controller.h>
#include <robot_mechanism_controllers/joint_velocity_controller.h>
#include <boost/scoped_ptr.hpp>

namespace controller
{

class HysteresisController : public Controller
{

public:
  enum { STOPPED, STARTING, MOVING, ANALYZING, DONE};

  HysteresisController();
  ~HysteresisController();

  /*!
   * \brief Functional way to initialize.
   * \param *robot The robot that is being controlled.
   * \param &n NodeHandle of mechanism control
   */
  bool init(mechanism::RobotState *robot, const ros::NodeHandle &n);

  /*!
   * \brief Called when controller is started
   */
  bool starting();

  /*!
   * \brief Perform the test analysis
   */
  void analysis();

  /*!
   * \brief Issues commands to the joint to perform hysteresis test.
   */
  void update();

  /*! 
   * \brief Sends data, returns true if sent
   */
  bool sendData();
  
  bool done() { return state_ == DONE; }
  

private:
  joint_qualification_controllers::TestData::Request test_data_;

  mechanism::JointState *joint_;     /**< Joint we're controlling. */
  mechanism::RobotState *robot_;     /**< Pointer to robot structure. */
  controller::JointVelocityController *velocity_controller_;    /**< The velocity controller for the hysteresis test. */
  double velocity_;            /**< Velocity during the test. */
  double max_effort_;          /**< Maximum allowable effort. */
  double initial_time_;             /**< Start time of the test. */
  double initial_position_;
  int count_;
  int loop_count_;
  bool complete;
  bool start;

  double timeout_;
  
  int state_;
  int starting_count;

  bool data_sent_;
  
  double last_publish_time_;

  // RT service call
  boost::scoped_ptr<realtime_tools::RealtimeSrvCall<joint_qualification_controllers::TestData::Request, joint_qualification_controllers::TestData::Response> > call_service_;

};
}


