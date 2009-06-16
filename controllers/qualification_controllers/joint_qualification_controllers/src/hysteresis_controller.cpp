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
#include <joint_qualification_controllers/hysteresis_controller.h>

#define MAX_DATA_POINTS 120000

using namespace std;
using namespace controller;

ROS_REGISTER_CONTROLLER(HysteresisController)

HysteresisController::HysteresisController():
  joint_(NULL), robot_(NULL)
{
  test_data_.test_name ="hysteresis";
  test_data_.joint_name = "default joint";
  test_data_.time.resize(MAX_DATA_POINTS);
  test_data_.cmd.resize(MAX_DATA_POINTS);
  test_data_.effort.resize(MAX_DATA_POINTS);
  test_data_.position.resize(MAX_DATA_POINTS);
  test_data_.velocity.resize(MAX_DATA_POINTS);

  test_data_.arg_name.resize(11);
  test_data_.arg_name[0] = "min_expected_effort";
  test_data_.arg_name[1] = "max_expected_effort";
  test_data_.arg_name[2] = "min_pos";
  test_data_.arg_name[3] = "max_pos";
  test_data_.arg_name[4] = "search_vel";
  test_data_.arg_name[5] = "timeout";
  test_data_.arg_name[6] = "slope";
  test_data_.arg_name[7] = "p_gain";
  test_data_.arg_name[8] = "i_gain";
  test_data_.arg_name[9] = "d_gain";
  test_data_.arg_name[10] = "iClamp";

  test_data_.arg_value.resize(11);
  
  state_         = STOPPED;
  starting_count = 0;
  velocity_      = 0;
  initial_time_  = 0;
  max_effort_    = 0;
  complete       = false;
  start          = true;
  loop_count_    = 0;
  count_         = 0;
  
  timeout_ = MAX_DATA_POINTS / 1000;
}

HysteresisController::~HysteresisController()
{
}

void HysteresisController::init( double velocity, double max_effort, double max_expected_effort, double min_expected_effort, double min_pos, double max_pos, double time, double timeout, double slope, std::string name, mechanism::RobotState *robot)
{
  assert(robot);
  robot_ = robot;
  joint_ = robot->getJointState(name);

  velocity_ = velocity;
  max_effort_ = max_effort;
  initial_time_ = time;
  initial_position_ = joint_->position_;
  timeout_ = timeout;

  // Set values in test data output
  test_data_.joint_name = name;
  test_data_.arg_value[0] = min_expected_effort;
  test_data_.arg_value[1] = max_expected_effort;
  test_data_.arg_value[2] = min_pos;
  test_data_.arg_value[3] = max_pos;
  test_data_.arg_value[4] = velocity;
  test_data_.arg_value[5] = timeout;
  test_data_.arg_value[6] = slope;

  // Get the gains, add them to test data
  double p, i, d, iClamp, imin;

  velocity_controller_->getGains(p, i, d, iClamp, imin);

  test_data_.arg_value[7] = p;
  test_data_.arg_value[8] = i;
  test_data_.arg_value[9] = d;
  test_data_.arg_value[10] = iClamp;

  
}

bool HysteresisController::initXml(mechanism::RobotState *robot, TiXmlElement *config)
{
  assert(robot);
  
  TiXmlElement *j = config->FirstChildElement("joint");
  if (!j)
  {
    fprintf(stderr, "HysteresisController was not given a joint\n");
    return false;
  }

  const char *joint_name = j->Attribute("name");
  joint_ = joint_name ? robot->getJointState(joint_name) : NULL;
  if (!joint_)
  {
    fprintf(stderr, "HysteresisController could not find joint named \"%s\"\n", joint_name);
    return false;
  }
  ///\todo check velocity controller comes up
  velocity_controller_ = new JointVelocityController();
  velocity_controller_->initXml(robot, config);

  TiXmlElement *cd = j->FirstChildElement("controller_defaults");
  if (cd)
  {
    double velocity = atof(cd->Attribute("velocity"));
    double max_effort = atof(cd->Attribute("max_effort"));
    double max_expected_effort = atof(cd->Attribute("max_expected_effort"));
    double min_expected_effort = atof(cd->Attribute("min_expected_effort"));
    double min_pos = atof(cd->Attribute("min_pos"));
    double max_pos = atof(cd->Attribute("max_pos"));
    
    // Pull timeout attribute if it exists
    const char *time_char = cd->Attribute("timeout");
    double timeout = time_char ? atof(cd->Attribute("timeout")) : timeout_;

    const char *slope_char = cd->Attribute("slope");
    double slope = slope_char ? atof(cd->Attribute("slope")) : 0;



    init(velocity, max_effort, max_expected_effort, min_expected_effort, min_pos, max_pos, robot->hw_->current_time_, timeout, slope, j->Attribute("name"), robot);
  }
  else
  {
    fprintf(stderr, "HysteresisController's config did not specify the default control parameters.\n");
    return false;
  }
  return true;
}

void HysteresisController::update()
{
  // wait until the joint is calibrated if it has limits
  if(!joint_->calibrated_ && joint_->joint_->type_!=mechanism::JOINT_CONTINUOUS)
  {
    return;
  }

  double time = robot_->hw_->current_time_;
  velocity_controller_->update();
  
  if (state_ == STOPPED || state_ == STARTING || state_ == MOVING)
  {
    if(state_!=DONE && count_ < MAX_DATA_POINTS && loop_count_ > 1)
    {
      double cmd;
      velocity_controller_->getCommand(cmd);
      
      test_data_.time[count_] = time;
      test_data_.cmd[count_] = cmd;
      test_data_.effort[count_] = joint_->applied_effort_;
      test_data_.position[count_] = joint_->position_;
      test_data_.velocity[count_] = joint_->velocity_;
      count_++;
    }
  }

  // Timeout check.
  if (time - initial_time_ > timeout_ && state_ != ANALYZING && state_ != DONE) 
  {
    state_ = ANALYZING;
    test_data_.arg_value[5] = 0;
  }

  switch (state_)
  {
  case STOPPED:
    velocity_controller_->setCommand(velocity_);
    velocity_ *= -1.0;
    ++loop_count_;
    starting_count = 0;
    state_ = STARTING;
    break;
  case STARTING:
    
    ++starting_count;
    if (starting_count > 100)
      state_ = MOVING;
    break;
  case MOVING:
    if (fabs(joint_->velocity_) < 0.001 && fabs(joint_->commanded_effort_) > max_effort_ && joint_->joint_->type_!=mechanism::JOINT_CONTINUOUS)
    {
      velocity_controller_->setCommand(0.0);
      if (loop_count_ < 3)
        state_ = STOPPED;
      else
        state_ = ANALYZING;
    }
    else if(fabs(joint_->position_-initial_position_)>6.28 && joint_->joint_->type_==mechanism::JOINT_CONTINUOUS) 
    {
   
      velocity_controller_->setCommand(0.0);
      initial_position_=joint_->position_;
      if (loop_count_ < 3)
        state_ = STOPPED;
      else
        state_ = ANALYZING;
    }
    break;
  case ANALYZING:
    velocity_controller_->setCommand(0.0);
    analysis();
    state_ = DONE;
    break;
  case DONE:
    velocity_controller_->setCommand(0.0);
    break;
  }

}

void HysteresisController::analysis()
{
  //test done
  if (count_ == 0)
    count_ = 1; // Resize if no points

  test_data_.time.resize(count_);
  test_data_.cmd.resize(count_);
  test_data_.effort.resize(count_);
  test_data_.position.resize(count_);
  test_data_.velocity.resize(count_);
  
  return; 
}

ROS_REGISTER_CONTROLLER(HysteresisControllerNode)
HysteresisControllerNode::HysteresisControllerNode()
: data_sent_(false), last_publish_time_(0), call_service_("/test_data")
{
  c_ = new HysteresisController();
}

HysteresisControllerNode::~HysteresisControllerNode()
{
  delete c_;
}

void HysteresisControllerNode::update()
{
  c_->update();
  if (c_->done())
  {
    if(!data_sent_)
    {
      if (call_service_.trylock())
      {
        joint_qualification_controllers::TestData::Request *out = &call_service_.srv_req_;
        out->test_name = c_->test_data_.test_name;
        out->joint_name = c_->test_data_.joint_name;

        out->time = c_->test_data_.time;
        out->cmd = c_->test_data_.cmd;
        out->effort = c_->test_data_.effort;
        out->position = c_->test_data_.position;
        out->velocity = c_->test_data_.velocity;

        out->arg_name = c_->test_data_.arg_name;
        out->arg_value = c_->test_data_.arg_value;
        call_service_.unlockAndCall();
        data_sent_ = true;
      }
    }
  }

  
}

bool HysteresisControllerNode::initXml(mechanism::RobotState *robot, TiXmlElement *config)
{
  assert(robot);
  robot_ = robot;
  
  if (!c_->initXml(robot, config))
    return false;
    
  return true;
}

