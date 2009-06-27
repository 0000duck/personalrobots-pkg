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
/*! \namespace controller
 \brief The controller namespace

 \class controller::Controller
 \brief A base level controller class.

 */
/***************************************************/

#include <loki/Factory.h>
#include <mechanism_model/robot.h>

#include <tinyxml/tinyxml.h>
#include <std_srvs/Empty.h>
#include <ros/node.h>
#include <ros/node_handle.h>

namespace controller
{

class Controller;
typedef Loki::SingletonHolder
<
  Loki::Factory< Controller, std::string >,
  Loki::CreateUsingNew,
  Loki::LongevityLifetime::DieAsSmallObjectParent
> ControllerFactory;

#define ROS_REGISTER_CONTROLLER(c) \
  controller::Controller *ROS_New_##c() { return new c(); }             \
  class RosController##c { \
  public: \
    RosController##c() \
    { \
      controller::ControllerFactory::Instance().Register(#c, ROS_New_##c); \
    } \
    ~RosController##c() \
    { \
      controller::ControllerFactory::Instance().Unregister(#c); \
    } \
  }; \
  static RosController##c ROS_CONTROLLER_##c;


class Controller
{
public:
  enum {CONSTRUCTED, INITIALIZED, RUNNING};
  int state_;

  Controller()
  {
    state_ = CONSTRUCTED;
  }
  virtual ~Controller()
  {
  }

  // The starting method is called by the realtime thread just before
  // the first call to update.
  virtual bool starting() { return true; }
  virtual void update(void) = 0;
  virtual bool stopping() {return true;}
  virtual bool initXml(mechanism::RobotState *robot, TiXmlElement *config) = 0;
  virtual bool init(mechanism::RobotState *robot, const ros::NodeHandle &n) { return false; };

  bool isRunning()
  {
    return (state_ == RUNNING);
  }

  void updateRequest()
  {
    if (state_ == RUNNING)
      update();
  }

  bool startRequest()
  {
    bool ret = false;

    // start succeeds even if the controller was already started
    if (state_ == RUNNING || state_ == INITIALIZED){
      ret = starting();
      if (ret) state_ = RUNNING;
    }

    return ret;
  }


  bool stopRequest()

  {
    bool ret = false;

    // stop succeeds even if the controller was already stopped
    if (state_ == RUNNING || state_ == INITIALIZED){
      stopping();
      state_ = INITIALIZED;
    }

    return ret;
  }

  bool initRequest(mechanism::RobotState *robot, const ros::NodeHandle &n)
  {
    if (state_ != CONSTRUCTED)
      return false;
    else
    {
      // initialize
      if (!init(robot, n))
        return false;
      state_ = INITIALIZED;

      return true;
    }
  }

  bool initXmlRequest(mechanism::RobotState *robot, TiXmlElement *config, std::string controller_name)
  {
    if (state_ != CONSTRUCTED)
      return false;
    else
    {
      // initialize
      if (!initXml(robot, config))
        return false;
      state_ = INITIALIZED;

      return true;
    }
  }

private:
  Controller(const Controller &c);
  Controller& operator =(const Controller &c);

};

}
