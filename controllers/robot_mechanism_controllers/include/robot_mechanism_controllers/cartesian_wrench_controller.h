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
 * Author: Wim Meeussen
 */

#ifndef CARTESIAN_WRENCH_CONTROLLER_H
#define CARTESIAN_WRENCH_CONTROLLER_H

#include <vector>
#include <kdl/chain.hpp>
#include <kdl/frames.hpp>
#include <kdl/chainjnttojacsolver.hpp>
#include <ros/node.h>
#include <robot_msgs/Wrench.h>
#include <mechanism_model/controller.h>
#include <mechanism_model/chain.h>
#include <tf/transform_datatypes.h>
#include <diagnostic_msgs/DiagnosticMessage.h>
#include <realtime_tools/realtime_publisher.h>
#include <boost/scoped_ptr.hpp>


namespace controller {

class CartesianWrenchController : public Controller
{
public:
  CartesianWrenchController();
  ~CartesianWrenchController();

  bool initXml(mechanism::RobotState *robot_state, TiXmlElement *config);

  bool starting();
  void update();

  void command();

  // input of the controller
  KDL::Wrench wrench_desi_;

private:
  bool publishDiagnostics(int level, const std::string& message);

  ros::Node* node_;
  std::string controller_name_;
  mechanism::RobotState *robot_state_;
  mechanism::Chain chain_;

  KDL::Chain kdl_chain_;
  boost::scoped_ptr<KDL::ChainJntToJacSolver> jnt_to_jac_solver_;
  KDL::JntArray jnt_pos_, jnt_eff_;
  KDL::Jacobian jacobian_;

  diagnostic_msgs::DiagnosticMessage diagnostics_;
  realtime_tools::RealtimePublisher <diagnostic_msgs::DiagnosticMessage> diagnostics_publisher_;
  ros::Time diagnostics_time_;
  ros::Duration diagnostics_interval_;

  robot_msgs::Wrench wrench_msg_;

  struct joint_constraint{
    int joint;
    double low_limit;
    double high_limit;
    double stiffness;
  };

  joint_constraint constraint_;

};

} // namespace


#endif
