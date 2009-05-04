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

## A basic node to listen to and display incoming diagnostic messages

import roslib
roslib.load_manifest('led_demo')

import sys
import rospy
from robot_msgs.msg import *

NAME = 'led_status_display'

def diagnostic_callback(message):
    print""
    print "New Message at %.1f"%message.header.stamp.to_time()
    for s in message.status:
        ## @TODO process byte level
        print "Name: %s \nMessage: %s"%(s.name, s.message)
        for v in s.strings + s.values:
            print "   %s: %s" % (v.label, v.value)
    sys.stdout.flush()

def battery_callback(state):
  if((state.energy_remaining / state.energy_capacity) <= 0.6):
    BATTERY_LEVEL = ceil((1-(state.energy_remaining / state.energy_capacity))*48)
  if((state.energy_remaining / state.energy_capacity) <= 0.2):
    BATTERY_URGENCY = ceil(((state.energy_remaining / state.energy_capacity)/0.2)*7)
  
    
def listener():
    rospy.Subscriber("/diagnostics", DiagnosticMessage, diagnostic_callback)
    rospy.Subscriber("/battery_state", BatteryState, battery_callback)
    rospy.init_node(NAME, anonymous=True)
    rospy.spin()
        
if __name__ == '__main__':
    try:
        listener()
    except KeyboardInterrupt, e:
        pass
    print "exiting"
