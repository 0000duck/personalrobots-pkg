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

import roslib
roslib.load_manifest('visual_odometry')

import sys
import time
import getopt

from math import *

import rospy

import stereo_msgs.msg
import deprecated_msgs.msg

class diff:
  def __init__(self):
    self.prev_a = 0
    self.prev_b = 0
    rospy.Subscriber('/stereo/raw_stereo', stereo_msgs.msg.RawStereo, self.handle_a, queue_size=2, buff_size=7000000)
    rospy.Subscriber('/vo', deprecated_msgs.msg.VOPose, self.handle_b, queue_size=2, buff_size=7000000)

  def handle_a(self, msg):
    self.prev_a = msg.header.stamp.to_seconds()
    print "A difference", abs(self.prev_b - self.prev_a)

  def handle_b(self, msg):
    self.prev_b = msg.header.stamp.to_seconds()
    print "B difference", abs(self.prev_b - self.prev_a)

def main(args):
  rospy.init_node('diff')
  D = diff()
  rospy.spin()

if __name__ == '__main__':
  main(sys.argv)
