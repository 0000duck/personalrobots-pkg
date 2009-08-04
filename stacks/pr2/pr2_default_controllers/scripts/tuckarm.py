#!/usr/bin/env python
# Software License Agreement (BSD License)
#
# Copyright (c) 2009, Willow Garage, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#  * Neither the name of the Willow Garage nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Modified by Kevin Watts for two arm use

import roslib
roslib.load_manifest('pr2_default_controllers')

import rospy
import os

from manipulation_msgs.msg import JointTraj, JointTrajPoint
from mechanism_control import mechanism
from robot_mechanism_controllers.srv import *

import sys
from time import sleep

def go(side, positions):
  pub = rospy.Publisher(side + '_arm_joint_trajectory_controller/command', JointTraj)

  # HACK
  sleep(1)

  msg = JointTraj()
  msg.points = []
  for i in range(0,len(positions)):
    msg.points.append(JointTrajPoint())
    msg.points[i].positions = positions[i]
    msg.points[i].time = 0.0

  pub.publish(msg)

USAGE = 'tuckarm.py <arms> ; <arms> is \'(r)ight\', \'(l)eft\', or \'(b)oth\' arms'

def set_params_right():
  rospy.set_param("r_arm_joint_trajectory_controller/type", "JointTrajectoryController")
  rospy.set_param("r_arm_joint_trajectory_controller/velocity_scaling_factor", 0.5)
  rospy.set_param("r_arm_joint_trajectory_controller/trajectory_wait_timeout", 0.25)

  rospy.set_param("r_arm_joint_trajectory_controller/r_shoulder_pan_joint/goal_reached_threshold", 0.1)
  rospy.set_param("r_arm_joint_trajectory_controller/r_shoulder_lift_joint/goal_reached_threshold", 0.1)
  rospy.set_param("r_arm_joint_trajectory_controller/r_upperarm_roll_joint/goal_reached_threshold", 0.1)
  rospy.set_param("r_arm_joint_trajectory_controller/r_elbow_flex_joint/goal_reached_threshold", 0.1)
  rospy.set_param("r_arm_joint_trajectory_controller/r_forearm_roll_joint/goal_reached_threshold", 0.1)
  rospy.set_param("r_arm_joint_trajectory_controller/r_wrist_flex_joint/goal_reached_threshold", 0.1)
  rospy.set_param("r_arm_joint_trajectory_controller/r_wrist_roll_joint/goal_reached_threshold", 0.1)

def set_params_left():
  rospy.set_param("l_arm_joint_trajectory_controller/type", "JointTrajectoryController")
  rospy.set_param("l_arm_joint_trajectory_controller/velocity_scaling_factor", 0.5)
  rospy.set_param("l_arm_joint_trajectory_controller/trajectory_wait_timeout", 0.25)

  rospy.set_param("l_arm_joint_trajectory_controller/l_shoulder_pan_joint/goal_reached_threshold", 0.1)
  rospy.set_param("l_arm_joint_trajectory_controller/l_shoulder_lift_joint/goal_reached_threshold", 0.1)
  rospy.set_param("l_arm_joint_trajectory_controller/l_upperarm_roll_joint/goal_reached_threshold", 0.1)
  rospy.set_param("l_arm_joint_trajectory_controller/l_elbow_flex_joint/goal_reached_threshold", 0.1)
  rospy.set_param("l_arm_joint_trajectory_controller/l_forearm_roll_joint/goal_reached_threshold", 0.1)
  rospy.set_param("l_arm_joint_trajectory_controller/l_wrist_flex_joint/goal_reached_threshold", 0.1)
  rospy.set_param("l_arm_joint_trajectory_controller/l_wrist_roll_joint/goal_reached_threshold", 0.1)


if __name__ == '__main__':
  if len(sys.argv) < 2:
    print USAGE
    sys.exit(-1)

  side = sys.argv[1]
  
  rospy.wait_for_service('spawn_controller')
  rospy.init_node('tuck_arms', anonymous = True)

  # Load xml file for arm trajectory controllers
  path = roslib.packages.get_pkg_dir('pr2_default_controllers')

  xml_for_left = open(os.path.join(path, 'l_arm_joint_trajectory_controller.xml'))
  xml_for_right = open(os.path.join(path, 'r_arm_joint_trajectory_controller.xml'))
  set_params_left()
  rospy.set_param('l_arm_joint_trajectory_controller/xml', xml_for_left.read())
  set_params_right()
  rospy.set_param('r_arm_joint_trajectory_controller/xml', xml_for_right.read())
  
  controllers = []
  try:
    if side == 'l' or side == 'left':
      # tuck traj for left arm
      mechanism.spawn_controller('l_arm_joint_trajectory_controller', True)
      controllers.append('l_arm_joint_trajectory_controller')

      positions = [[0.4,0.0,0.0,-2.25,0.0,0.0,0.0], [0.0,1.57,1.57,-2.25,0.0,0.0,0.0]]  
      go('l', positions)

      rospy.spin()

    elif side == 'r' or side == 'right':
      # tuck traj for right arm
      mechanism.spawn_controller('r_arm_joint_trajectory_controller', True)
      controllers.append('r_arm_joint_trajectory_controller')
      positions = [[-0.4,0.0,0.0,-1.57,0.0,0.0,0.0], [0.0,1.57,-1.57,-1.57,0.0,0.0,0.0]]    
      go('r', positions)

      rospy.spin()

    elif side == 'b' or side == 'both':
      # Both arms
      # Holds left arm up at shoulder lift
      mechanism.spawn_controller('r_arm_joint_trajectory_controller', True)
      mechanism.spawn_controller('l_arm_joint_trajectory_controller', True)

      controllers.append('r_arm_joint_trajectory_controller')
      controllers.append('l_arm_joint_trajectory_controller')
        
      positions_l = [[0.4,0.0,0.0,-2.25,0.0,0.0,0.0], [0.0,.90,1.57,-2.25,0.0,0.0,0.0]] 
      positions_r = [[-0.4,0.0,0.0,-2.25,0.0,0.0,0.0], [0.0,1.57,-1.57,-1.57,0.0,0.0,0.0]]
      
      go('r', positions_r)
      sleep(0.5)
      go('l', positions_l)

      rospy.spin()

    else:
      print 'Unknown side! Must be l/left, r/right, or b/both!'
      print USAGE
      sys.exit(2)
  finally:
    for name in controllers:
      for i in range(0,3):
        try:
          mechanism.kill_controller(name)
          break
        except:
          pass
