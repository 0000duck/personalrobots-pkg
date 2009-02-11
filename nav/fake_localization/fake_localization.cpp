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

/** \author Ioan Sucan */

/**

@mainpage

@htmlinclude manifest.html

@b odom_localization is simply forwards the odometry information 

<hr>

@section usage Usage
@verbatim
$ odom_localization
@endverbatim

<hr>

@section topic ROS topics

Subscribes to (name/type):
- @b "base_pose_ground_truth"/robot_msgs::PoseWithRatesStamped : robot's odometric pose.  Only the position information is used (velocity is ignored).
- @b "initialpose"/Pose2DFloat32 : robot's odometric pose.  Only the position information is used (velocity is ignored).

Publishes to (name / type):
- @b "localizedpose"/RobotBase2DOdom : robot's localized map pose.  Only the position information is set (no velocity).
- @b "particlecloud"/ParticleCloud : fake set of particles being maintained by the filter (one paricle only).

<hr>

@section parameters ROS parameters

- None

 **/

#include <ros/node.h>
#include <ros/time.h>

#include <deprecated_msgs/RobotBase2DOdom.h>
#include <robot_msgs/PoseWithRatesStamped.h>
#include <robot_msgs/ParticleCloud.h>
#include <deprecated_msgs/Pose2DFloat32.h>

#include <angles/angles.h>

#include "ros/console.h"

#include "tf/transform_broadcaster.h"


class FakeOdomNode: public ros::Node
{
public:
    FakeOdomNode(void) : ros::Node("fake_localization")
    {
      advertise<deprecated_msgs::RobotBase2DOdom>("localizedpose",1);
      advertise<robot_msgs::ParticleCloud>("particlecloud",1);

      m_tfServer = new tf::TransformBroadcaster(*this);	

      m_lastUpdate = ros::Time::now();
      
      m_base_pos_received = false;

      m_iniPos.x = m_iniPos.y = m_iniPos.th = 0.0;
      m_particleCloud.set_particles_size(1);
      subscribe("base_pose_ground_truth", m_basePosMsg, &FakeOdomNode::basePosReceived,1);
    }
    
    ~FakeOdomNode(void)
    {
      if (m_tfServer)
        delete m_tfServer; 
    }
   

  // Just kill time as spin is not working!
    void run(void)
    {
      // A duration for sleeping will be 100 ms
      ros::Duration snoozer(0, 100000000);

      while(true){
	snoozer.sleep();
      }
    }  
    
private:
    
    tf::TransformBroadcaster       *m_tfServer;
    ros::Time                      m_lastUpdate;
    double                         m_maxPublishFrequency;
    bool                           m_base_pos_received;
    
    robot_msgs::PoseWithRatesStamped  m_basePosMsg;
    robot_msgs::ParticleCloud      m_particleCloud;
    deprecated_msgs::RobotBase2DOdom      m_currentPos;
    deprecated_msgs::Pose2DFloat32        m_iniPos;
    
    void basePosReceived(void)
    {
      update();
    }

  void update(){
    tf::Transform txi(tf::Quaternion(m_basePosMsg.pos.orientation.x,
				     m_basePosMsg.pos.orientation.y, 
				     m_basePosMsg.pos.orientation.z, 
				     m_basePosMsg.pos.orientation.w),
		      tf::Point(m_basePosMsg.pos.position.x,
				m_basePosMsg.pos.position.y,
                                0.0*m_basePosMsg.pos.position.z )); // zero height for base_footprint

    double x = txi.getOrigin().x() + m_iniPos.x;
    double y = txi.getOrigin().y() + m_iniPos.y;
    double z = txi.getOrigin().z();

    double yaw, pitch, roll;
    txi.getBasis().getEulerZYX(yaw, pitch, roll);
    yaw = angles::normalize_angle(yaw + m_iniPos.th);

    tf::Transform txo(tf::Quaternion(yaw, pitch, roll), tf::Point(x, y, z));
    
    //tf::Transform txIdentity(tf::Quaternion(0, 0, 0), tf::Point(0, 0, 0));
    
    // Here we directly publish a transform from Map to base_link. We will skip the intermediate step of publishing a transform
    // to the base footprint, as it seems unnecessary. However, if down the road we wish to use the base footprint data,
    // and some other component is publishing it, this should change to publish the map -> base_footprint instead.
    // A hack links the two frames.
    // m_tfServer->sendTransform(tf::Stamped<tf::Transform>
    //                           (txIdentity.inverse(),
    //                            m_basePosMsg.header.stamp,
    //                            "base_footprint", "base_link"));  // this is published by base controller
    m_tfServer->sendTransform(tf::Stamped<tf::Transform>
			      (txo.inverse(),
			       m_basePosMsg.header.stamp,
			       "map", "base_footprint"));

    // Publish localized pose
    m_currentPos.header = m_basePosMsg.header;
    m_currentPos.pos.x = x;
    m_currentPos.pos.y = y;
    m_currentPos.pos.th = yaw;
    publish("localizedpose", m_currentPos);

    // The particle cloud is the current position. Quite convenient.
    robot_msgs::Pose pos;
    tf::PoseTFToMsg(tf::Pose(tf::Quaternion(m_currentPos.pos.th, 0, 0), tf::Vector3(m_currentPos.pos.x, m_currentPos.pos.y, 0)),
                    pos);
    m_particleCloud.particles[0] = pos;
    publish("particlecloud", m_particleCloud);
  }   
};

int main(int argc, char** argv)
{
    ros::init(argc, argv);
    
    FakeOdomNode odom;
    odom.spin();

    
    
    return 0;
}
