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

## Gazebo test base controller vw

PKG = 'test_pr2_mechanism_controllers_gazebo'
NAME = 'test_base_odomx_gt'

import math
import roslib
roslib.load_manifest(PKG)

import sys, unittest
import os, time
import rospy, rostest

from geometry_msgs.msg import Twist,Vector3
from nav_msgs.msg import Odometry

TEST_DURATION   = 10.0

TARGET_VX       =  0.5
TARGET_VY       =  0.0
TARGET_VW       =  0.0
TARGET_DURATION = 2.0
TARGET_TOL      = 1.0 #empirical test result john - 20081124

class E:
    def __init__(self,x,y,z):
        self.x = x
        self.y = y
        self.z = z

class Q:
    def __init__(self,x,y,z,u):
        self.x = x
        self.y = y
        self.z = z
        self.u = u
    def normalize(self):
        s = math.sqrt(self.u * self.u + self.x * self.x + self.y * self.y + self.z * self.z)
        self.u /= s
        self.x /= s
        self.y /= s
        self.z /= s
    def getEuler(self):
        self.normalize()
        squ = self.u
        sqx = self.x
        sqy = self.y
        sqz = self.z
        # init euler angles
        vec = E(0,0,0)
        # Roll
        vec.x = math.atan2(2 * (self.y*self.z + self.u*self.x), squ - sqx - sqy + sqz);
        # Pitch
        vec.y = math.asin(-2 * (self.x*self.z - self.u*self.y));
        # Yaw
        vec.z = math.atan2(2 * (self.x*self.y + self.u*self.z), squ + sqx - sqy - sqz);
        return vec



class BaseTest(unittest.TestCase):
    def __init__(self, *args):
        super(BaseTest, self).__init__(*args)
        self.success = False
        self.reached_target_vw = False
        self.duration_start = 0

        self.odom_xi = 0;
        self.odom_yi = 0;
        self.odom_ti = 0;
        self.odom_initialized = False;

        self.odom_x = 0;
        self.odom_y = 0;
        self.odom_t = 0;

        self.p3d_xi = 0;
        self.p3d_yi = 0;
        self.p3d_ti = 0;
        self.p3d_initialized = False;

        self.p3d_x = 0;
        self.p3d_y = 0;
        self.p3d_t = 0;


    def printBaseOdom(self, odom):
        orientation = odom.pose.orientation
        q = Q(orientation.x, orientation.y, orientation.z, orientation.w)
        q.normalize()
        print "odom received"
        print "odom pos " + "x: " + str(odom.pose.pose.position.x)
        print "odom pos " + "y: " + str(odom.pose.pose.position.y)
        print "odom pos " + "t: " + str(q.getEuler().z)
        print "odom vel " + "x: " + str(odom.twist.twist.linear.x)
        print "odom vel " + "y: " + str(odom.twist.twist.linear.y)
        print "odom vel " + "t: " + str(odom.twist.twist.angular.z)

    def printBaseP3D(self, p3d):
        print "base pose ground truth received"
        print "P3D pose translan: " + "x: " + str(p3d.pose.pose.position.x)
        print "                   " + "y: " + str(p3d.pose.pose.position.y)
        print "                   " + "z: " + str(p3d.pose.pose.position.z)
        print "P3D pose rotation: " + "x: " + str(p3d.pose.pose.orientation.x)
        print "                   " + "y: " + str(p3d.pose.pose.orientation.y)
        print "                   " + "z: " + str(p3d.pose.pose.orientation.z)
        print "                   " + "w: " + str(p3d.pose.pose.orientation.w)
        print "P3D rate translan: " + "x: " + str(p3d.twist.twist.linear.x)
        print "                   " + "y: " + str(p3d.twist.twist.linear.y)
        print "                   " + "z: " + str(p3d.twist.twist.linear.z)
        print "P3D rate rotation: " + "x: " + str(p3d.twist.twist.angular.vx)
        print "                   " + "y: " + str(p3d.twist.twist.angular.vy)
        print "                   " + "z: " + str(p3d.twist.twist.angular.vz)

    def odomInput(self, odom):
        #self.printBaseOdom(odom)
        orientation = odom.pose.orientation
        q = Q(orientation.x, orientation.y, orientation.z, orientation.w)
        q.normalize()
        if self.odom_initialized == False:
            self.odom_initialized = True
            self.odom_xi = odom.pose.pose.position.x
            self.odom_yi = odom.pose.pose.position.y
            self.odom_ti = q.getEuler().z
        self.odom_x = odom.pose.pose.position.x   - self.odom_xi
        self.odom_y = odom.pose.pose.position.y   - self.odom_yi
        self.odom_t = q.getEuler().z  - self.odom_ti


    def p3dInput(self, p3d):
        q = Q(p3d.pose.pose.orientation.x , p3d.pose.pose.orientation.y , p3d.pose.pose.orientation.z , p3d.pose.pose.orientation.w)
        q.normalize()
        v = q.getEuler()

        if self.p3d_initialized == False:
            self.p3d_initialized = True
            self.p3d_xi = p3d.pose.pose.position.x
            self.p3d_yi = p3d.pose.pose.position.y
            self.p3d_ti = v.z

        self.p3d_x = p3d.pose.pose.position.x - self.p3d_xi
        self.p3d_y = p3d.pose.pose.position.y - self.p3d_yi
        self.p3d_t = v.z                - self.p3d_ti

    
    def test_base(self):
        print "LNK\n"
        pub = rospy.Publisher("/cmd_vel", Twist)
        rospy.Subscriber("/base_pose_ground_truth", Odometry, self.p3dInput)
        rospy.Subscriber("/odom",                   Odometry, self.odomInput)
        rospy.init_node(NAME, anonymous=True)
        timeout_t = time.time() + TEST_DURATION
        while not rospy.is_shutdown() and not self.success and time.time() < timeout_t:
            pub.publish(Twist(Vector3(TARGET_VX,TARGET_VY, 0), Vector3(0,0,TARGET_VW)))
            time.sleep(0.1)
            # display what odom thinks
            #print " odom    " + " x: " + str(self.odom_x) + " y: " + str(self.odom_y) + " t: " + str(self.odom_t)
            # display what ground truth is
            #print " p3d     " + " x: " + str(self.p3d_x)  + " y: " + str(self.p3d_y)  + " t: " + str(self.p3d_t)
            # display what the odom error is
            print " error   " + " x: " + str(self.odom_x - self.p3d_x) + " y: " + str(self.odom_y - self.p3d_y) + " t: " + str(self.odom_t - self.p3d_t)

        # check total error
        total_error = abs(self.odom_x - self.p3d_x) + abs(self.odom_y - self.p3d_y) + abs(self.odom_t - self.p3d_t)
        if total_error < TARGET_TOL:
            self.success = True

        self.assert_(self.success)
        
if __name__ == '__main__':
    rostest.run(PKG, sys.argv[0], BaseTest, sys.argv) #, text_mode=True)


