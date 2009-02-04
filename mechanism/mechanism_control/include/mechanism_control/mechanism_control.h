
///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2008-2009, Willow Garage, Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//   * Neither the name of Stanford University nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////

/*
 * Author: Stuart Glaser
 */
#ifndef MECHANISM_CONTROL_H
#define MECHANISM_CONTROL_H

#include <pthread.h>
#include <map>
#include <string>
#include <vector>
#include "ros/node.h"
#include "roslib/Time.h"
#include <tinyxml/tinyxml.h>
#include <hardware_interface/hardware_interface.h>
#include <mechanism_model/robot.h>
#include <boost/circular_buffer.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <mechanism_model/controller.h>
#include <realtime_tools/realtime_publisher.h>
#include <misc_utils/advertised_service_guard.h>

#include <robot_srvs/ListControllerTypes.h>
#include <robot_srvs/ListControllers.h>
#include <robot_srvs/SpawnController.h>
#include <robot_srvs/KillController.h>
#include <robot_msgs/MechanismState.h>
#include <robot_msgs/DiagnosticMessage.h>
#include "tf/tfMessage.h"

typedef controller::Controller* (*ControllerAllocator)();

class MechanismControl {
public:
  MechanismControl(HardwareInterface *hw);
  virtual ~MechanismControl();

  // Real-time functions
  void update();

  // Non real-time functions
  bool initXml(TiXmlElement* config);
  void getControllerNames(std::vector<std::string> &v);
  bool spawnController(const std::string &type, const std::string &name, TiXmlElement *config);
  bool killController(const std::string &name);
  controller::Controller* getControllerByName(std::string name);

  mechanism::Robot model_;
  mechanism::RobotState *state_;
  HardwareInterface *hw_;

private:
  bool initialized_;

  const static int MAX_NUM_CONTROLLERS = 100;
  boost::mutex controllers_lock_;
  controller::Controller* controllers_[MAX_NUM_CONTROLLERS];
  std::string controller_names_[MAX_NUM_CONTROLLERS];

  typedef boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::max, boost::accumulators::tag::mean, boost::accumulators::tag::variance> > TimeStatistics;
  struct {
    TimeStatistics acc_;
    double max_;
    boost::circular_buffer<double> max1_;
    struct {
      TimeStatistics acc_;
      double max_;
      boost::circular_buffer<double> max1_;
    } controllers_[MAX_NUM_CONTROLLERS];
  } diagnostics_;
  realtime_tools::RealtimePublisher<robot_msgs::DiagnosticMessage> publisher_;
  void publishDiagnostics();

  // Killing a controller:
  // 1. Non-realtime thread places the index of the controller into please_remove_
  // 2. Realtime thread moves the controller out of the array and into removed_
  // 3. Non-realtime thread can now delete the controller
  // The non-realtime thread will hold controllers_lock_ throughout the process.
  // please_remove_ must be -1 when not removing
  int please_remove_;
  controller::Controller* removed_;
};

/*
 * Exposes MechanismControl's interface over ROS
 */
class MechanismControlNode
{
public:
  MechanismControlNode(MechanismControl *mc);
  virtual ~MechanismControlNode();

  bool initXml(TiXmlElement *config);

  void update();  // Must be realtime safe

  bool listControllerTypes(robot_srvs::ListControllerTypes::Request &req,
                           robot_srvs::ListControllerTypes::Response &resp);
  bool listControllers(robot_srvs::ListControllers::Request &req,
                       robot_srvs::ListControllers::Response &resp);
  bool spawnController(robot_srvs::SpawnController::Request &req,
                       robot_srvs::SpawnController::Response &resp);

private:
  ros::Node *node_;

  bool killController(robot_srvs::KillController::Request &req,
                      robot_srvs::KillController::Response &resp);

  MechanismControl *mc_;

  //static const double STATE_PUBLISHING_PERIOD = 0.01;  // this translates to about 100Hz
  static const int CYCLES_PER_STATE_PUBLISH = 10;  // 100 Hz
  int cycles_since_publish_;

  const char* const mechanism_state_topic_;
  realtime_tools::RealtimePublisher<robot_msgs::MechanismState> publisher_;

  realtime_tools::RealtimePublisher<tf::tfMessage> transform_publisher_;

  AdvertisedServiceGuard list_controllers_guard_, list_controller_types_guard_,
    spawn_controller_guard_, kill_controller_guard_;
};

#endif /* MECHANISM_CONTROL_H */
