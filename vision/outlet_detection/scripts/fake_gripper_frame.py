#! /usr/bin/env python
# Copyright (c) 2008, Willow Garage, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the Willow Garage, Inc. nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import roslib
roslib.load_manifest('outlet_detection')
import rospy
from robot_msgs.msg import *
from tf.msg import tfMessage
from math import *
import time

def xyz(x, y, z):
  p = Vector3()
  p.x, p.y, p.z = x, y, z
  return p

def rpy(y, p, r):
  q = Quaternion()
  cosY = cos(y / 2. + pi/2)
  sinY = sin(y / 2. + pi/2)
  cosP = cos(p / 2.)
  sinP = sin(p / 2.)
  cosR = cos(r / 2.)
  sinR = sin(r / 2.)
  q.x = cosR*cosP*cosY + sinR*sinP*sinY
  q.y = sinR*cosP*cosY + cosR*sinP*sinY
  q.z = cosR*sinP*cosY + sinR*cosP*sinY
  q.w = cosR*cosP*sinY + sinR*sinP*cosY
  return q

class Tracker:
  def __init__(self, topic, Msg):
    self.sub = rospy.Subscriber(topic, Msg, self.callback)
    self.msg = Msg()

  def callback(self, msg):
    self.msg = msg

t = TransformStamped()
t.header.frame_id = 'r_gripper_tool_frame'
t.header.seq = 0
#t.parent_id = 'base_link'
#t.transform.translation = xyz(0.506305, -0.194843, 0.284381)
#t.transform.rotation = rpy(0, 1.13, 0)
#t.transform.rotation = Quaternion(0.050555, 0.245594, -0.051239, 0.966697)
t.parent_id = 'high_def_frame'
t.transform.translation = xyz(1.013218, -0.019657, -0.044208)
t.transform.rotation = Quaternion(-0.092009, -0.482387, 0.139255, 0.859910)

rospy.init_node('fake_gripper')
pub_tf = rospy.Publisher('/tf_message', tfMessage)

while not rospy.is_shutdown():
  t.header.stamp = rospy.rostime.get_rostime()
  msg = tfMessage([t])
  pub_tf.publish(msg)
  time.sleep(0.1)
