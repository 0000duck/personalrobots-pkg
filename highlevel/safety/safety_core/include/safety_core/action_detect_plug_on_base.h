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
 *   * Neither the name of Willow Garage nor the names of its
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


/* Author: Melonee Wise */

#ifndef ACTION_DETECT_PLUG_ON_BASE_H
#define ACTION_DETECT_PLUG_ON_BASE_H


#include <ros/node.h>
#include <plugs_msgs/PlugStow.h>
#include <pr2_msgs/SetPeriodicCmd.h>
#include <std_msgs/Empty.h>
#include <robot_actions/action.h>
#include <plug_onbase_detector/plug_onbase_detector.h>

namespace safety_core{

class DetectPlugOnBaseAction: public robot_actions::Action<std_msgs::Empty, plugs_msgs::PlugStow>
{
public:
  DetectPlugOnBaseAction(ros::Node& node);
  virtual ~DetectPlugOnBaseAction();

  robot_actions::ResultStatus execute(const std_msgs::Empty& empty, plugs_msgs::PlugStow& feedback);

private:
  // average the last couple plug centroids
  void localizePlug();
  void reset();
  
  std::string action_name_;
  
  ros::Node& node_;

  PlugOnBaseDetector::PlugOnBaseDetector* detector_;

  plugs_msgs::PlugStow plug_stow_;
  plugs_msgs::PlugStow plug_stow_msg;
  
  std::string laser_controller_;
  int not_found_count_;
  int found_count_;

  double sum_x_;
  double sum_y_;
  double sum_z_;

  double sum_sq_x_;
  double sum_sq_y_;
  double sum_sq_z_;

  double std_x_;
  double std_y_;
  double std_z_;

  pr2_msgs::SetPeriodicCmd::Request req_laser_;
  pr2_msgs::SetPeriodicCmd::Response res_laser_;
};

}
#endif
