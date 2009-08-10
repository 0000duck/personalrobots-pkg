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

/**
 * @file This class provides a simple C++ client interface to use a robot action over ros. It is intended
 * to help action developers test actions together under simple scenarios to validate behavior as much as possible
 * prior to more advanced integration.
 *
 * @author Conor McGann
 */

#ifndef H_robot_actions_ActionClient
#define H_robot_actions_ActionClient

#include "robot_actions/action.h"
#include <std_msgs/Empty.h>

namespace robot_actions {

  template <class Goal, class State, class Feedback> class ActionClient {

  public:

    /**
     * @brief Construct the action with a name. It should match the server name. This name is used to scope
     * ros topics used in communication
     */
    ActionClient(const std::string& name);

    /**
     * @brief ROS Cleanup
     */
    ~ActionClient();

    /**
     * @brief Executes the action, with a duration bound. Will preempt the server if we have not
     * terminated by the given time limit.
     */
    ResultStatus execute(const Goal& goal, Feedback& feedback, const ros::Duration& duration_bound);

    /**
     * @brief Executes the action, with no duration limit. Will run to completion by the server.
     */
    ResultStatus execute(const Goal& goal, Feedback& feedback);

    void preempt();


  private:
    void callbackHandler(const boost::shared_ptr<State const> &input); // Just a no-op
    State _state_update_msg;
    const std::string _goal_topic;
    const std::string _preempt_topic;
    const std::string _feedback_topic;
    bool _is_active;
    ros::NodeHandle _node;
    ros::Publisher _goal_publisher, _preempt_publisher;
    ros::Subscriber _feedback_subscriber;
  };

  template <class Goal, class State, class Feedback>
    void ActionClient<Goal, State, Feedback>::callbackHandler(const boost::shared_ptr<State const>& input) {
    _state_update_msg = *input;
    _is_active = (_state_update_msg.status.value == _state_update_msg.status.ACTIVE);
  }


  template <class Goal, class State, class Feedback>
  ActionClient<Goal, State, Feedback>::ActionClient(const std::string& name)
    : _goal_topic(name + "/activate"), _preempt_topic(name + "/preempt"), _feedback_topic(name + "/feedback"),_is_active(false){

    // Subscribe to state updates
    _feedback_subscriber = _node.subscribe(_feedback_topic, 1, &ActionClient<Goal, State, Feedback>::callbackHandler, this);

    // Advertize goal requests. 
    _goal_publisher = _node.advertise<Goal>(_goal_topic, 1);

    // Advertize goal preemptions.
    _preempt_publisher = _node.advertise<std_msgs::Empty>(_preempt_topic, 1);

    //Wait until a non-zero time
    while (ros::Time::now() == ros::Time(0.0)) {
      ros::Duration(0.001).sleep();
    }

    // wait until we have at least 1 subscriber per advertised topic
    ros::Time start_time = ros::Time::now();
    ros::Duration timeout(10.0);
    while (_goal_publisher.getNumSubscribers() < 1 || _preempt_publisher.getNumSubscribers() < 1 ){
      if (ros::Time::now() > start_time+timeout){
	ROS_ERROR("Action client did not receive subscribers on the %s or %s topic", _goal_topic.c_str(), _preempt_topic.c_str());
	break;
      }
      ros::Duration(0.1).sleep();
    }
  }

  template <class Goal, class State, class Feedback>
    ActionClient<Goal, State, Feedback>::~ActionClient(){
  }

  template <class Goal, class State, class Feedback>
  ResultStatus ActionClient<Goal, State, Feedback>::execute(const Goal& goal, Feedback& feedback, const ros::Duration& duration_bound){
    static const ros::Duration NO_DURATION_BOUND;

    const ros::Time start_time = ros::Time::now();

    ResultStatus result = robot_actions::PREEMPTED;

    // Post the goal
    _goal_publisher.publish(goal);

    // Wait for activation. If we do not activate in time, we will preempt. Will not block for preemption
    // callback since the action may be bogus.
    ros::Duration SLEEP_DURATION(0.001);
    while(!_is_active){
      SLEEP_DURATION.sleep();

      // Check if we need to preempt. If we do, we publish the preemption but will not terminate until
      // inactive
      if(duration_bound != NO_DURATION_BOUND){
	ros::Duration elapsed_time = ros::Time::now() - start_time;
	if(elapsed_time > duration_bound){
	  _preempt_publisher.publish(std_msgs::Empty());
	  return result;
	}
      }
    }

    // Wait for completion
    while(_is_active){
      // Check if we need to preempt. If we do, we publish the preemption but will not terminate until
      // inactive
      if(duration_bound != NO_DURATION_BOUND){
	ros::Duration elapsed_time = ros::Time::now() - start_time;
	if(elapsed_time > duration_bound){
	  _preempt_publisher.publish(std_msgs::Empty());
	}
      }

      SLEEP_DURATION.sleep();
    }

    // Obtain result data
    _state_update_msg.lock();
    feedback = _state_update_msg.feedback;
    result = static_cast<ResultStatus>(_state_update_msg.status.value);
    _state_update_msg.unlock();

    return result;
  }
  
  template <class Goal, class State, class Feedback>
  ResultStatus ActionClient<Goal, State, Feedback>::execute(const Goal& goal, Feedback& feedback){
    return execute(goal, feedback, ros::Duration());
  }

  template <class Goal, class State, class Feedback>
  void ActionClient<Goal, State, Feedback>::preempt(){
    _preempt_publisher.publish(std_msgs::Empty());
  }

}
#endif
