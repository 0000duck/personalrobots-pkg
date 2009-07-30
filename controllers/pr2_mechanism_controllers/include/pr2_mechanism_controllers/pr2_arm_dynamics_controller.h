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

#include <ros/node.h>
#include <boost/thread/mutex.hpp>

#include <mechanism_control/controller.h>
#include <mechanism_model/joint.h>
#include <robot_mechanism_controllers/joint_position_controller.h>
#include <robot_mechanism_controllers/joint_velocity_controller.h>
#include <robot_mechanism_controllers/joint_effort_controller.h>

// Services
#include <robot_srvs/SetJointCmd.h>
#include <robot_srvs/GetJointCmd.h>

#include <pr2_mechanism_controllers/SetJointGains.h>
#include <pr2_mechanism_controllers/GetJointGains.h>

//Kinematics
#include <robot_kinematics/robot_kinematics.h>

// #include <urdf/URDF.h>

namespace controller
{

// The maximum number of joints expected in an arm.
static const int MAX_ARM_JOINTS = 7;

class PR2ArmDynamicsController : public Controller
{
public:

  /*!
   * \brief Default Constructor of the JointController class.
   *
   */
  PR2ArmDynamicsController();

  /*!
   * \brief Destructor of the JointController class.
   */
  virtual ~PR2ArmDynamicsController();

  /*!
   * \brief Functional way to initialize limits and gains.
   *
  */
  bool initXml(mechanism::RobotState *robot, TiXmlElement *config);


  /*!
   * \brief Sets a goal for some joint, specified as a pair of (joint values, joint names)
   *
   * \param j_values the positions of the joints
   * \param j_names the names of the joints
  */
  void setJointCmd(const std::vector<double> &j_values, const std::vector<double> &j_values_dot, const std::vector<double> &j_values_dot_dot, const std::vector<std::string> & j_names);

//  /**
//   * @brief Overloaded method for convenience
//   * @param req
//   */
//  void setJointCmd(robot_msgs::SetJointPosCmd::Request &req);

  void getJointCmd(robot_msgs::JointCmd & cmd) const;

  /*!
   * \brief Get latest position command to the joint: revolute (angle) and prismatic (position).
  */
//  void getJointPosCmd(pr2_mechanism_controllers::GetJointPosCmd::Response &resp);

//  void getCurrentConfiguration(std::vector<double> &);

  /*!
     * \brief Issues commands to the joint. Should be called at regular intervals
  */
  virtual void update(void); // Real time safe.

  boost::mutex arm_controller_lock_;

//  void setJointGains(const pr2_mechanism_controllers::SetJointGains::Request &req);

//  void getJointGains(pr2_mechanism_controllers::GetJointGains::Response &resp);

  controller::JointEffortController* getJointEffortControllerByName(std::string name);

  bool goalAchieved() const { return goal_achieved_; }

  robot_kinematics::RobotKinematics pr2_kin_;

  robot_kinematics::SerialChain *arm_chain_;

  robot_kinematics::RobotKinematics pr2_kin_uncompensated_;

  robot_kinematics::SerialChain *arm_chain_uncompensated_;

  unsigned int num_joints_;

private:

  double last_time_;

  std::vector<control_toolbox::Pid> pid_controllers_;

  std::vector<JointEffortController *> joint_effort_controllers_;

  // Goal of the joints
  std::vector<double> goals_;

  // Goal of the joints
  std::vector<double> goals_dot_;

  // Goal of the joints
  std::vector<double> goals_dot_dot_;

  // Goal of the joints - used by the realtime code only
  std::vector<double> goals_rt_;

  // Goal of the joints - used by the realtime code only
  std::vector<double> goals_rt_dot_;

  // Goal of the joints - used by the realtime code only
  std::vector<double> goals_rt_dot_dot_;

  mechanism::Robot* robot_;

  void updateJointControllers(void);

  int getJointControllerByName(std::string name);

  void checkForGoalAchieved_(void);

  bool goal_achieved_;

  KDL::Vector *gravity_torque_;

  KDL::Vector *gravity_torque_uncompensated_;

  std::vector<double> control_torque_;

  void computeControlTorque(const double &time);

  // Indicates if goals_ and error_margins_ should be copied into goals_rt_ and error_margins_rt_
  bool refresh_rt_vals_;

};

/** @class PR2ArmDynamicsControllerNode
 *  @brief ROS interface for the arm controller.
 *  @author Timothy Hunter <tjhunter@willowgarage.com>
 *
 *  This class provides a ROS interface for controlling the arm by setting position configurations. If offers several ways to control the arms:
 *  - through listening to ROS messages: this is specified in the XML configuration file by the following parameters:
 *      <listen_topic name="the name of my message" />
 *      (only one topic can be specified)
 *  - through a non blocking service call: this service call can specify a single configuration as a target (and maybe multiple configuration in the future)
 *  - through a blocking service call: this service can receive a list of position commands that will be followed one after the other
 *
 */
class PR2ArmDynamicsControllerNode : public Controller
{
  public:
    /*!
    * \brief Default Constructor
    *
    */
    PR2ArmDynamicsControllerNode();

    /*!
    * \brief Destructor
    */
    ~PR2ArmDynamicsControllerNode();

    void update();

    bool initXml(mechanism::RobotState *robot, TiXmlElement *config);

    /** @brief sets a command for all the joints managed by the controller at once
     * This is a lightweight version of setJointPosHeadless when the caller knows the order of the joints.
     * @note this service should not be used. use setJointPosHeadless instead
     * This service returns immediately
     * @param req an array of position
     * @param resp The response is empty
     * @return
     */
    bool setJointSrv(robot_srvs::SetJointCmd::Request &req,
                     robot_srvs::SetJointCmd::Response &resp);

    /** @brief service that returns the goal of the controller
     * @note if you know the goal has been reached and you do not want to subscribe to the /mechanism_state topic, you can use it as a hack to get the position of the arm
     * @param req
     * @param resp the response, contains a JointPosCmd message with the goal of the controller
     * @return
     */
    bool getJointCmd(robot_srvs::GetJointCmd::Request &req,
                     robot_srvs::GetJointCmd::Response &resp);

  private:
    robot_msgs::JointCmd msg_;   //The message used by the ROS callback
    PR2ArmDynamicsController *c_;

};

}


