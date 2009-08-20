/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
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
/*
 * teleop_goal_projection.cpp
 *
 *  Created on: Jun 29, 2009
 *      Author: Matthew Piccoli
 */

#include <ros/node.h>
#include <geometry_msgs/PoseStamped.h>
#include <std_msgs/Float64.h>
#include "joy/Joy.h"
#include <ros/time.h>
#include "tf/transform_listener.h"

#include "mechanism_msgs/JointState.h"
#include "mechanism_msgs/JointStates.h"

#define TORSO_TOPIC "/torso_lift_controller/set_command"
#define HEAD_TOPIC "/head_controller/command"

class TeleopGoalProjection
{
  public:
    geometry_msgs::PoseStamped cmd_;
    std_msgs::Float64 torso_vel;
    joy::Joy joy;
    double req_vx, req_vy, req_vw, req_torso, req_pan, req_tilt;
    double max_vx, max_vy, max_vw;
    double max_pan, max_tilt, min_tilt, pan_step, tilt_step;
    int axis_vx, axis_vy, axis_vw, axis_pan, axis_tilt;
    int deadman_button, run_button, torso_dn_button, torso_up_button, head_button, passthrough_button;
    bool deadman_no_publish_, torso_publish, head_publish, deadman_pressed_previous_iteration;
    ros::Time last_recieved_joy_message_time_, last_sent_command_time_;
    ros::Duration joy_msg_timeout_;
    ros::Duration send_cmd_time_interval_;
    tf::TransformListener tf;
    std::string global_frame_;
    std::string base_frame_;
    ros::NodeHandle node_handle_;

    ros::Publisher torso_pub_,goal_pub_,head_pub_;
    ros::Subscriber joy_sub_;
    // Set pan, tilt steps as params

    TeleopGoalProjection(bool deadman_no_publish = false):pan_step(0.1), tilt_step(0.1), deadman_no_publish_(deadman_no_publish)
    {
      torso_vel.data = 0;
      cmd_.pose.position.x = cmd_.pose.position.y = cmd_.pose.orientation.z = 0;
      req_pan = req_tilt = 0;
      last_sent_command_time_ = ros::Time::now();
      node_handle_.param("~max_vx", max_vx, 0.6);
      node_handle_.param("~max_vw", max_vw, 0.8);
      node_handle_.param("~max_vy", max_vy, 0.6);

      node_handle_.param("~max_pan", max_pan, 2.7);
      node_handle_.param("~max_tilt", max_tilt, 1.4);
      node_handle_.param("~min_tilt", min_tilt, -0.4);

      node_handle_.param("~tilt_step", tilt_step, 0.1);
      node_handle_.param("~pan_step", pan_step, 0.1);

      node_handle_.param("~global_frame", global_frame_, std::string("/map"));
      node_handle_.param("~base_frame", base_frame_, std::string("/base_link"));

      node_handle_.param<int> ("~axis_pan", axis_pan, 0);
      node_handle_.param<int> ("~axis_tilt", axis_tilt, 2);

      node_handle_.param<int> ("~axis_vx", axis_vx, 3);
      node_handle_.param<int> ("~axis_vw", axis_vw, 0);
      node_handle_.param<int> ("~axis_vy", axis_vy, 2);

      node_handle_.param<int> ("~deadman_button", deadman_button, 0);
      node_handle_.param<int> ("~torso_dn_button", torso_dn_button, 0);
      node_handle_.param<int> ("~torso_up_button", torso_up_button, 0);
      node_handle_.param<int> ("~head_button", head_button, 0);

      double send_cmd_hz;
      node_handle_.param ("~send_cmd_hz", send_cmd_hz, -1.0); //As fast as possible
      if(send_cmd_hz <= 0)
      {
        send_cmd_time_interval_ = ros::Duration().fromSec(0.0);//DURATION_MAX;
        ROS_DEBUG("send_cmd_time_interval_ <= 0 -> send commands as fast as possible");
      }
      else
      {
        send_cmd_time_interval_.fromSec(1.0/send_cmd_hz);
        ROS_INFO("send_cmd_time_interval: %.3f", send_cmd_time_interval_.toSec());
      }

      double joy_msg_timeout;
      node_handle_.param<double> ("~joy_msg_timeout", joy_msg_timeout, -1.0); //default to no timeout
      if(joy_msg_timeout <= 0)
      {
        joy_msg_timeout_ = ros::Duration().fromSec(9999999);//DURATION_MAX;
        ROS_DEBUG("joy_msg_timeout <= 0 -> no timeout");
      }
      else
      {
        joy_msg_timeout_.fromSec(joy_msg_timeout);
        ROS_DEBUG("joy_msg_timeout: %.3f", joy_msg_timeout_.toSec());
      }

      ROS_DEBUG("max_vx: %.3f m/s\n", max_vx);
      ROS_DEBUG("max_vy: %.3f m/s\n", max_vy);
      ROS_DEBUG("max_vw: %.3f deg/s\n", max_vw*180.0/M_PI);

      ROS_DEBUG("tilt step: %.3f rad\n", tilt_step);
      ROS_DEBUG("pan step: %.3f rad\n", pan_step);

      ROS_DEBUG("axis_vx: %d\n", axis_vx);
      ROS_DEBUG("axis_vy: %d\n", axis_vy);
      ROS_DEBUG("axis_vw: %d\n", axis_vw);
      ROS_DEBUG("axis_pan: %d\n", axis_pan);
      ROS_DEBUG("axis_tilt: %d\n", axis_tilt);

      ROS_DEBUG("deadman_button: %d\n", deadman_button);
      ROS_DEBUG("run_button: %d\n", run_button);
      ROS_DEBUG("torso_dn_button: %d\n", torso_dn_button);
      ROS_DEBUG("torso_up_button: %d\n", torso_up_button);
      ROS_DEBUG("head_button: %d\n", head_button);
      ROS_DEBUG("passthrough_button: %d\n", passthrough_button);
      ROS_DEBUG("joy_msg_timeout: %f\n", joy_msg_timeout);

      if(torso_dn_button != 0)
        torso_pub_ = node_handle_.advertise<std_msgs::Float64> (TORSO_TOPIC, 1);
      if(head_button != 0)
        head_pub_ = node_handle_.advertise<mechanism_msgs::JointStates> (HEAD_TOPIC, 1);
      goal_pub_ = node_handle_.advertise<geometry_msgs::PoseStamped> ("/goal", 1);
      joy_sub_ = node_handle_.subscribe("joy",1,&TeleopGoalProjection::joy_cb,this);
      ROS_DEBUG("done with ctor\n");
    }

    ~TeleopGoalProjection()     
    {
    }

    void joy_cb(const joy::JoyConstPtr &joy_msg)
    {
      joy = *joy_msg;
      //Record this message reciept
      last_recieved_joy_message_time_ = ros::Time::now();

      /*
       printf("axes: ");
       for(int i=0;i<joy.get_axes_size();i++)
       printf("%.3f ", joy.axes[i]);
       puts("");
       printf("buttons: ");
       for(int i=0;i<joy.get_buttons_size();i++)
       printf("%d ", joy.buttons[i]);
       puts("");
       */
      bool cmd_head = (((unsigned int) head_button < joy.get_buttons_size()) && joy.buttons[head_button]);

      bool deadman = (((unsigned int) deadman_button < joy.get_buttons_size()) && joy.buttons[deadman_button]);

      // Base
      if((axis_vx >= 0) && (((unsigned int) axis_vx) < joy.get_axes_size()) && !cmd_head)
        req_vx = joy.axes[axis_vx] * max_vx;
      else
        req_vx = 0.0;
      if((axis_vy >= 0) && (((unsigned int) axis_vy) < joy.get_axes_size()) && !cmd_head)
        req_vy = joy.axes[axis_vy] * max_vy;
      else
        req_vy = 0.0;
      if((axis_vw >= 0) && (((unsigned int) axis_vw) < joy.get_axes_size()) && !cmd_head)
        req_vw = joy.axes[axis_vw] * max_vw;
      else
        req_vw = 0.0;

      // Head
      // Update commanded position by how joysticks moving
      // Don't add commanded position if deadman off
      if((axis_pan >= 0) && (((unsigned int) axis_pan) < joy.get_axes_size()) && cmd_head && deadman)
      {
        req_pan += joy.axes[axis_pan] * pan_step;
        req_pan = std::max(std::min(req_pan, max_pan), -max_pan);
      }

      if((axis_tilt >= 0) && (((unsigned int) axis_tilt) < joy.get_axes_size()) && cmd_head && deadman)
      {
        req_tilt += joy.axes[axis_tilt] * tilt_step;
        req_tilt = std::max(std::min(req_tilt, max_tilt), min_tilt);
      }

      // Torso
      bool down = (((unsigned int) torso_dn_button < joy.get_buttons_size()) && joy.buttons[torso_dn_button]);
      bool up = (((unsigned int) torso_up_button < joy.get_buttons_size()) && joy.buttons[torso_up_button]);

      // Bring torso up/down with max effort
      if(down && !up)
        req_torso = -0.01;
      else if(up && !down)
        req_torso = 0.01;
      else
        req_torso = 0;

    }
    void passthrough_cb()
    {
    }
    void send_cmd_vel()
    {
      ROS_INFO("Inside send cmd %f %f %f",send_cmd_time_interval_.toSec(),last_sent_command_time_.toSec(),ros::Time::now().toSec());
      if(last_sent_command_time_ + send_cmd_time_interval_ < ros::Time::now())
      {
        ROS_INFO("Trying to send command");
        //ROS_INFO("last %f, interval %f, now %f", last_sent_command_time_.toSec(), send_cmd_time_interval_.toSec(), ros::Time::now().toSec());
        //ROS_INFO("last %f, interval %f, now %f", last_recieved_joy_message_time_.toSec(), joy_msg_timeout_.toSec(), ros::Time::now().toSec());
        joy.lock();
        if(((deadman_button < 0) || ((((unsigned int) deadman_button) < joy.get_buttons_size()) && joy.buttons[deadman_button])) && last_recieved_joy_message_time_ + joy_msg_timeout_ > ros::Time::now())
        {
          deadman_pressed_previous_iteration = true;
          //ROS_INFO("!!!");
          // use commands from the local sticks
          //TODO::convert relative frame (base?) to global frame
          geometry_msgs::PoseStamped local_cmd;
          local_cmd.pose.position.x = req_vx;
          local_cmd.pose.position.y = req_vy;
          local_cmd.pose.orientation.z = req_vw;
          local_cmd.pose.orientation.w = sqrt(1 - local_cmd.pose.orientation.z * local_cmd.pose.orientation.z);
          local_cmd.header.frame_id = base_frame_;
          local_cmd.header.stamp = ros::Time();
          try
          {
            tf.transformPose(global_frame_, local_cmd, cmd_);
          }
          catch(tf::LookupException& ex) {
            ROS_ERROR("No Transform available Error: %s\n", ex.what());
            joy.unlock();
            return;
          }
          catch(tf::ConnectivityException& ex) {
            ROS_ERROR("Connectivity Error: %s\n", ex.what());
            joy.unlock();
            return;
          }
          catch(tf::ExtrapolationException& ex) {
            ROS_ERROR("Extrapolation Error: %s\n", ex.what());
            joy.unlock();
            return;
          }

          cmd_.header.stamp = ros::Time::now();
          goal_pub_.publish(cmd_);

          // Torso
          torso_vel.data = req_torso;
          if(torso_dn_button != 0)
            torso_pub_.publish(torso_vel);

          // Head
          if(head_button != 0)
          {
	    mechanism_msgs::JointState joint_cmd ;
	    mechanism_msgs::JointStates joint_cmds;

	    joint_cmd.name ="head_pan_joint";
	    joint_cmd.position = req_pan;
	    joint_cmds.joints.push_back(joint_cmd);
	    joint_cmd.name="head_tilt_joint";
	    joint_cmd.position = req_tilt;
	    joint_cmds.joints.push_back(joint_cmd);
            head_pub_.publish(joint_cmds);
          }

          /*if(req_torso != 0)
            fprintf(stderr, "teleop_goal_projection:: %f, %f, %f. Head:: %f, %f. Torso effort: %f.\n", cmd_.pose.position.x, cmd_.pose.position.y, cmd_.pose.orientation.z, req_pan, req_tilt, torso_vel.data);
          else
            fprintf(stderr, "teleop_goal_projection:: %f, %f, %f. Head:: %f, %f\n", cmd_.pose.position.x, cmd_.pose.position.y, cmd_.pose.orientation.z, req_pan, req_tilt);*/
        }
        else if(deadman_pressed_previous_iteration)
        {
          geometry_msgs::PoseStamped local_cmd;
          local_cmd.pose.position.x = 0.0;
          local_cmd.pose.position.y = 0.0;
          local_cmd.pose.orientation.z = 0.0;
          local_cmd.pose.orientation.w = sqrt(1 - local_cmd.pose.orientation.z * local_cmd.pose.orientation.z);
          local_cmd.header.frame_id = base_frame_;
          local_cmd.header.stamp = ros::Time();

          try
          {
            tf.transformPose(global_frame_, local_cmd, cmd_);
          }
          catch(tf::LookupException& ex) {
            ROS_ERROR("No Transform available Error: %s\n", ex.what());
            joy.unlock();
            return;
          }
          catch(tf::ConnectivityException& ex) {
            ROS_ERROR("Connectivity Error: %s\n", ex.what());
            joy.unlock();
            return;
          }
          catch(tf::ExtrapolationException& ex) {
            ROS_ERROR("Extrapolation Error: %s\n", ex.what());
            joy.unlock();
            return;
          }
          goal_pub_.publish(cmd_);
          deadman_pressed_previous_iteration = false;
        }
        else
        {
          cmd_.pose.position.x = cmd_.pose.position.y = cmd_.pose.orientation.z = 0;
          torso_vel.data = 0;
          if(!deadman_no_publish_)
          {
            goal_pub_.publish(cmd_);//Only publish if deadman_no_publish is enabled
            if(torso_dn_button != 0)
              torso_pub_.publish(torso_vel);

            // Publish head
            if(head_button != 0)
            {
	      mechanism_msgs::JointState joint_cmd ;
	      mechanism_msgs::JointStates joint_cmds;

	      joint_cmd.name ="head_pan_joint";
	      joint_cmd.position = req_pan;
	      joint_cmds.joints.push_back(joint_cmd);
	      joint_cmd.name="head_tilt_joint";
	      joint_cmd.position = req_tilt;
	      joint_cmds.joints.push_back(joint_cmd);
              head_pub_.publish(joint_cmds);
            }

          }
        }
        last_sent_command_time_ = ros::Time::now();
        joy.unlock();
      }
      else
      {
        ROS_INFO("Not sending command");
      }
      ROS_INFO("Done sending command");
    }
};

int main(int argc, char **argv)
{
  ros::init(argc, argv, "teleop_goal_projection");
  ros::spinOnce();
  const char* opt_no_publish = "--deadman_no_publish";

  bool no_publish = false;
  for(int i = 1; i < argc; i++)
  {
    if(!strncmp(argv[i], opt_no_publish, strlen(opt_no_publish)))
      no_publish = true;
  }
  TeleopGoalProjection teleop_goal_projection(no_publish);
  ros::Duration sleep_time(0.25);
  while(true)
  {
    ROS_INFO("Sending out command");
    sleep_time.sleep();
    ros::spinOnce();
    teleop_goal_projection.send_cmd_vel();
  }

  exit(0);
  return 0;
}
