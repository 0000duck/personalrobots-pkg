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
# Revision $Id: rossync 3844 2009-02-16 19:49:10Z gerkey $

import roslib
roslib.load_manifest('tabletop_manipulation')
import rospy
from manipulation_msgs.msg import JointTraj, JointTrajPoint
from mechanism_msgs.msg import MechanismState

import sys
from math import *

class TuckArm:
  def __init__(self, side):
    self.done = False
    self.tolerance = 0.15
    self.side = side
    self.pub = rospy.Publisher(self.side + '_arm_trajectory_command', JointTraj)
    rospy.Subscriber('mechanism_state', MechanismState, self.mechCallback)
    # Ordering
    self.joint_names = {self.side[0] + '_shoulder_pan_joint' : 0,
                        self.side[0] + '_shoulder_lift_joint' : 1,
                        self.side[0] + '_upper_arm_roll_joint' : 2,
                        self.side[0] + '_elbow_flex_joint' : 3,
                        self.side[0] + '_forearm_roll_joint' : 4,
                        self.side[0] + '_wrist_flex_joint' : 5,
                        self.side[0] + '_wrist_roll_joint': 6}

    self.positions = [[],[]]
    # 2-step tuck trajectory, tailored for each arm
    for j in range(0,2):
      for i in range(0,len(self.joint_names)):
        self.positions[j].append(0.0)
      self.positions[j][self.joint_names[self.side[0] + '_elbow_flex_joint']] = -2.0

    if self.side == 'left':
      self.positions[1][self.joint_names[self.side[0] + '_shoulder_lift_joint']] = 1.57
      self.positions[1][self.joint_names[self.side[0] + '_upper_arm_roll_joint']] = 1.3
    else:
      self.positions[1][self.joint_names[self.side[0] + '_shoulder_lift_joint']] = 1.57
      self.positions[1][self.joint_names[self.side[0] + '_upper_arm_roll_joint']] = -1.3

    print self.positions

  # Surely this stuff exists in Python somewhere...
  def normalizeAngle(self, a):
    return atan2(sin(a),cos(a))
  def shortestAngularDistance(self, a, b):
    a = self.normalizeAngle(a)
    b = self.normalizeAngle(b)
    d1 = a-b
    d2 = 2*pi - abs(d1)
    if d1 > 0:
      d2 *= -1.0
    if abs(d1) < abs(d2):
      return d1
    else:
      return d2
  
  def mechCallback(self, msg):
    for j in self.joint_names:
      found = False
      for mj in msg.joint_states:
        if mj.name == j:
          found = True
          diff = self.shortestAngularDistance(mj.position,
                                              self.positions[-1][self.joint_names[j]])
          if  diff > self.tolerance:
            #print '%s: %f - %f = %f'%(j,mj.position,
            #                          self.positions[-1][self.joint_names[j]],
            #                          diff)
            self.done = False
            return
          break
      if not found:
        print "Couldn't find joint %s!"%(j)
        sys.exit(-1)
    self.done = True  

  def tuckArm(self):
    msg = JointTraj()
    msg.points = []
    for i in range(0,len(self.positions)):
      msg.points.append(JointTrajPoint())
      msg.points[i].positions = self.positions[i]
      msg.points[i].time = 0.0
  
    self.pub.publish(msg)

    while not self.done:
      print '[TuckArm] Waiting for goal achievement...'
      rospy.sleep(1.0)

    return True

USAGE = 'tuckarm.py {left|right}'

if __name__ == '__main__':
  if len(sys.argv) < 2 or (sys.argv[1] != 'left' and sys.argv[1] != 'right'):
    print USAGE
    sys.exit(-1)

  side = sys.argv[1]

  ta = TuckArm(side)
  rospy.init_node('tuckarm', anonymous=True)
  #HACK
  import time
  time.sleep(3.0)

  res = ta.tuckArm()

  if res:
    print 'Success!'
  else:
    print 'Failure!'
