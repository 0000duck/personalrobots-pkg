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

#include <kdl/frames.hpp>
#include <door_handle_detector/door_functions.h>
#include "doors_core/executive_functions.h"


using namespace KDL;
using namespace ros;
using namespace std;
using namespace tf;
using namespace door_handle_detector;




Stamped<Pose> getRobotPose(const robot_msgs::Door& door, double dist)
{
  Vector x_axis(1,0,0);

  // get vector from hinge to center of door
  Vector hinge;
  if (door.hinge == robot_msgs::Door::HINGE_P1)
    hinge = Vector(door.door_p1.x, door.door_p1.y, door.door_p1.z);
  else if (door.hinge == robot_msgs::Door::HINGE_P2)
    hinge = Vector(door.door_p2.x, door.door_p2.y, door.door_p2.z);
  else
    ROS_ERROR("GetRobotPose: door hinge side not specified");

  // calculate angle between the frame and x-axis
  Vector frame_vec(door.frame_p1.x - door.frame_p2.x, door.frame_p1.y - door.frame_p2.y, door.frame_p1.z - door.frame_p2.z);
  double angle_frame_x = getVectorAngle(x_axis, frame_vec);

  // get vector to center of frame
  double door_width = Vector(door.door_p1.x - door.door_p2.x, door.door_p1.y - door.door_p2.y, door.door_p1.z - door.door_p2.z).Norm();
  Vector frame_center = hinge + Vector(cos(angle_frame_x)*door_width/2.0, sin(angle_frame_x)*door_width/2.0, 0);

  // get normal on frame
  Vector normal_door(door.normal.x, door.normal.y, door.normal.z);
  Rotation rot_frame_door = Rotation::RotZ(getDoorAngle(door));
  Vector normal_frame = rot_frame_door * normal_door;
  double angle_normal_frame = getVectorAngle(x_axis, normal_frame);

  // get robot pos
  Vector robot_pos = frame_center - (normal_frame * dist);

  Stamped<Pose> robot_pose;
  robot_pose.frame_id_ = door.header.frame_id;
  robot_pose.setOrigin( Vector3(robot_pos(0), robot_pos(1), robot_pos(2)));
  robot_pose.setRotation( Quaternion(angle_normal_frame, 0, 0) ); 

  return robot_pose;  
}




