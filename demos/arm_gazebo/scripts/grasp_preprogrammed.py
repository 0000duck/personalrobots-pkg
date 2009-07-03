#!/usr/bin/env python
# Software License Agreement (BSD License)
#
# Copyright (c) 2008, Willow Garage, Inc.
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

## Gazebo tug arms for navigation

PKG = 'arm_gazebo'
NAME = 'grasp_preprogrammed'

import math
import roslib
roslib.load_manifest(PKG)
roslib.load_manifest('rostest')


import sys, unittest
import os, os.path, threading, time
import rospy, rostest
from robot_msgs.msg import *
from std_msgs.msg import *
from pr2_mechanism_controllers.msg import *


CMD_POS_1      =  0.0
CMD_POS_2      =  0.0
CMD_POS_3      =  0.0
CMD_POS_4      =  0.0
CMD_POS_5      =  0.0
CMD_POS_6      =  0.0
CMD_POS_7      =  0.0
LOWER_Z        =  0.1
PAN_RAD        =  0.2
G_OPEN         =  0.058;
G_CLOSE        =  0.0;

p3d_received   = False

def p3dReceived(stuff):
    global p3d_received
    if not p3d_received:
        p3d_received = True

if __name__ == '__main__':
    pub_r_shoulder_yaw   = rospy.Publisher("r_gripper_controller/set_command", Float64)
    pub_r_shoulder_pitch = rospy.Publisher("r_gripper_controller/set_command", Float64)
    pub_r_upper_arm_roll = rospy.Publisher("r_gripper_controller/set_command", Float64)
    pub_r_elbow_flex     = rospy.Publisher("r_gripper_controller/set_command", Float64)
    pub_r_forearm_roll   = rospy.Publisher("r_gripper_controller/set_command", Float64)
    pub_r_wrist_flex     = rospy.Publisher("r_gripper_controller/set_command", Float64)
    pub_r_wrist_roll     = rospy.Publisher("r_gripper_controller/set_command", Float64)
    pub_r_gripper        = rospy.Publisher("r_gripper_controller/set_command", Float64)
    rospy.Subscriber("r_gripper_palm_pose_ground_truth", PoseWithRatesStamped, p3dReceived)
    rospy.init_node(NAME, anonymous=True)


    #sleep for a while
    while not p3d_received:
      time.sleep(1)

    #open gripper
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_roll_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1,CMD_POS_2,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_OPEN))
    time.sleep(8)

    #lower arm
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_rolr_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1,CMD_POS_2+LOWER_Z,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_OPEN))
    time.sleep(2)

    #close gripper
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_rolr_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1,CMD_POS_2+LOWER_Z,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_CLOSE))
    time.sleep(8)

    #raise arm
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_roll_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1,CMD_POS_2,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_CLOSE))
    time.sleep(2)

    #pan arm
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_roll_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1+PAN_RAD,CMD_POS_2,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_CLOSE))
    time.sleep(2)

    #lower arm
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_roll_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1+PAN_RAD,CMD_POS_2+LOWER_Z,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_CLOSE))
    time.sleep(2)

    #open gripper
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_rolr_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1+PAN_RAD,CMD_POS_2+LOWER_Z,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_OPEN))
    time.sleep(8)

    #raise arm
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_roll_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1+PAN_RAD,CMD_POS_2,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_OPEN))
    time.sleep(2)

    #return arm
    pub_r_arm.publish(JointPosCmd(['r_shoulder_pan_joint','r_shoulder_lift_joint','r_upper_arm_roll_joint','r_elbow_flex_joint','r_forearm_roll_joint','r_wrist_flex_joint','r_wrist_roll_joint'],[CMD_POS_1,CMD_POS_2,CMD_POS_3,CMD_POS_4,CMD_POS_5,CMD_POS_6,CMD_POS_7],[0,0,0,0,0,0,0],0))
    pub_r_gripper.publish(Float64(G_OPEN))

