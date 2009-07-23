/*
 *  Gazebo - Outdoor Multi-Robot Simulator
 *  Copyright (C) 2003
 *     Nate Koenig & Andrew Howard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*
 * Desc: Controller for diffdrive robots in gazebo.
 * Author: Tony Pratkanis
 * Date: 17 April 2009
 * SVN info: $Id$
 */

#include <gazebo/gazebo.h>
#include <gazebo/GazeboError.hh>
#include <ros/ros.h>
#include "tf/transform_broadcaster.h"
#include <robot_msgs/PoseDot.h>
#include <deprecated_msgs/RobotBase2DOdom.h>

#include <boost/bind.hpp>

gazebo::PositionIface *posIface = NULL;

class DiffDrive {
public:
  gazebo::PositionIface *posIface;
  ros::NodeHandle* rnh_;
  ros::Subscriber  sub_;
  ros::Publisher   pub_;
  
  void cmdVelCallBack(const robot_msgs::PoseDot::ConstPtr& cmd_msg) {
        std::cout << " pos " << posIface
                  <<    " x " << cmd_msg->vel.vx
                  <<    " y " << cmd_msg->vel.vy
                  <<    " z " << cmd_msg->ang_vel.vz
                  << std::endl;
        
    if (posIface) {
      posIface->Lock(1);
      posIface->data->cmdVelocity.pos.x = cmd_msg->vel.vx;
      posIface->data->cmdVelocity.pos.y = cmd_msg->vel.vy;
      posIface->data->cmdVelocity.yaw = cmd_msg->ang_vel.vz;
      posIface->Unlock();
    }
  }

  DiffDrive() {
    gazebo::Client *client = new gazebo::Client();
    gazebo::SimulationIface *simIface = new gazebo::SimulationIface();
    posIface = new gazebo::PositionIface();
  
    int serverId = 0;
    
    /// Connect to the libgazebo server
    try {
      client->ConnectWait(serverId, GZ_CLIENT_ID_USER_FIRST);
    } catch (gazebo::GazeboError e) {
      std::cout << "Gazebo error: Unable to connect\n" << e << "\n";
      return;
    }
    
    /// Open the Simulation Interface
    try {
      simIface->Open(client, "default");
    } catch (gazebo::GazeboError e) {
      std::cout << "Gazebo error: Unable to connect to the sim interface\n" << e << "\n";
      return;
    }
    
    /// Open the Position interface
    try {
      posIface->Open(client, "robot_description::position_iface_0");
    } catch (std::string e) {
      std::cout << "Gazebo error: Unable to connect to the position interface\n" << e << "\n";
      return;
    }
    
    // Enable the motor
    posIface->Lock(1);
    posIface->data->cmdEnableMotors = 1;
    posIface->Unlock();

    this->rnh_ = new ros::NodeHandle();
    this->sub_ = rnh_->subscribe<robot_msgs::PoseDot>("/cmd_vel", 100, boost::bind(&DiffDrive::cmdVelCallBack,this,_1));
    this->pub_ = rnh_->advertise<deprecated_msgs::RobotBase2DOdom>("/odom", 1);
   
    deprecated_msgs::RobotBase2DOdom odom;
    tf::TransformBroadcaster tfb;

    ros::Duration d; d.fromSec(0.01);
    
    while(rnh_->ok()) { 
      if (posIface) {
        posIface->Lock(1);
        
        btQuaternion qt; qt.setEulerZYX(posIface->data->pose.yaw, posIface->data->pose.pitch, posIface->data->pose.roll);
        btVector3 vt(posIface->data->pose.pos.x, posIface->data->pose.pos.y, posIface->data->pose.pos.z);

        tf::Transform latest_tf(qt, vt);

        // We want to send a transform that is good up until a
        // tolerance time so that odom can be used
        ros::Time transform_expiration = ros::Time::now();
        tf::Stamped<tf::Transform> tmp_tf_stamped(latest_tf.inverse(),
                                                  transform_expiration,
                                                  "base_link", "odom");
        tfb.sendTransform(tmp_tf_stamped);
        

        odom.pos.x = posIface->data->pose.pos.x;
        odom.pos.y = posIface->data->pose.pos.y;
        odom.pos.th = posIface->data->pose.yaw;
        odom.vel.x = posIface->data->velocity.pos.x;
        odom.vel.y = posIface->data->velocity.pos.y;
        odom.vel.th = posIface->data->velocity.yaw;
        odom.stall = 0;
        
        odom.header.frame_id = "odom"; 
        
        odom.header.stamp = transform_expiration;
        
        this->pub_.publish(odom); 

        posIface->Unlock();
      }
      d.sleep();
    }
  }
  ~DiffDrive() {
    delete this->rnh_;
  }
};




int main(int argc, char** argv) {
  ros::init(argc,argv,"ros_diffdrive");

  // spawn 2 threads by default, ///@todo: make this a parameter
  ros::MultiThreadedSpinner s(1);
  boost::thread spinner_thread( boost::bind( &ros::spin, s ) );

  DiffDrive d;
  return 0;
}


