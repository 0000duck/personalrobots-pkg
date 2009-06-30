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

## Gazebo test 2dnav stack

PKG = 'test_2dnav_gazebo'
NAME = 'test_set_goal'

import math
import roslib
roslib.load_manifest(PKG)
roslib.load_manifest('rostest')


import sys, unittest
import os, os.path, threading, time
import rospy, rostest
from std_msgs.msg import *
from robot_actions.msg import *
from nav_robot_actions.msg import *
from robot_msgs.msg import *
from deprecated_msgs.msg import *
from tf.transformations import *
from numpy import *

FLOAT_TOL = 0.0001
AMCL_TOL  = 0.5
COV = [float64(0.5*0.5                  ),float64(0),float64(0),float64(0),float64(0),float64(0), \
       float64(0),float64(0.5*0.5                  ),float64(0),float64(0),float64(0),float64(0), \
       float64(0                        ),float64(0),float64(0),float64(0),float64(0),float64(0), \
       float64(0),float64(0),float64(0),float64(math.pi/23.0*math.pi/12.0),float64(0),float64(0), \
       float64(0                        ),float64(0),float64(0),float64(0),float64(0),float64(0), \
       float64(0                        ),float64(0),float64(0),float64(0),float64(0),float64(0)  ]

def normalize_angle_positive(angle):
    return math.fmod(math.fmod(angle, 2*math.pi) + 2*math.pi, 2*math.pi)

def normalize_angle(angle):
    anorm = normalize_angle_positive(angle)
    if anorm > math.pi:
      anorm -= 2*math.pi
    return anorm

def shortest_angular_distance(angle_from, angle_to):
    angle_diff = normalize_angle_positive(angle_to) - normalize_angle_positive(angle_from)
    if angle_diff > math.pi:
      angle_diff = -(2*math.pi - angle_diff)
    return normalize_angle(angle_diff)

class NavStackTest(unittest.TestCase):
    def __init__(self, *args):
        super(NavStackTest, self).__init__(*args)
        self.bumped  = False
        self.success = False

        self.publish_initialpose = False
        self.publish_goal = True

        self.odom_xi = 0
        self.odom_yi = 0
        self.odom_qi = [1,0,0,0]
        self.odom_initialized = False;

        self.odom_x = 0
        self.odom_y = 0
        self.odom_q = [1,0,0,0]

        self.p3d_xi = 0
        self.p3d_yi = 0
        self.p3d_qi = [1,0,0,0]
        self.p3d_initialized = False;

        self.p3d_x = 0
        self.p3d_y = 0
        self.p3d_q = [1,0,0,0]

        # default parameters
        self.nav_t_tol    = 0.1
        self.nav_xy_tol   = 0.7
        self.odom_t_tol   = 0.1
        self.odom_xy_tol  = 0.1
        self.test_timeout = 50.0
        # note: starting position of the robot is 25.70, 25.70 (center of map)
        self.target_x =  25.70
        self.target_y =  25.70
        self.target_t =  0.0

        self.args = sys.argv
        

    def printBaseOdom(self, odom):
        print "odom received"
        print "odom pos " + "x: " + str(odom.pos.x)
        print "odom pos " + "y: " + str(odom.pos.y)
        print "odom pos " + "t: " + str(odom.pos.th)
        print "odom vel " + "x: " + str(odom.vel.x)
        print "odom vel " + "y: " + str(odom.vel.y)
        print "odom vel " + "t: " + str(odom.vel.th)

    def printBaseP3D(self, p3d):
        print "base pose ground truth received"
        print "P3D pose translan: " + "x: " + str(p3d.pos.position.x)
        print "                   " + "y: " + str(p3d.pos.position.y)
        print "                   " + "z: " + str(p3d.pos.position.z)
        print "P3D pose rotation: " + "x: " + str(p3d.pos.orientation.x)
        print "                   " + "y: " + str(p3d.pos.orientation.y)
        print "                   " + "z: " + str(p3d.pos.orientation.z)
        print "                   " + "w: " + str(p3d.pos.orientation.w)
        print "P3D rate translan: " + "x: " + str(p3d.vel.vel.x)
        print "                   " + "y: " + str(p3d.vel.vel.y)
        print "                   " + "z: " + str(p3d.vel.vel.z)
        print "P3D rate rotation: " + "x: " + str(p3d.vel.ang_vel.vx)
        print "                   " + "y: " + str(p3d.vel.ang_vel.vy)
        print "                   " + "z: " + str(p3d.vel.ang_vel.vz)



    def odomInput(self, odom):
        #self.printBaseOdom(odom)
        # initialize odom
        if self.odom_initialized == False or self.p3d_initialized == False:
            self.odom_initialized = True
            self.odom_xi = odom.pos.x
            self.odom_yi = odom.pos.y
            self.odom_qi = quaternion_from_euler(0,0,odom.pos.th,'rxyz')
        else:
            # update odom
            self.odom_x = odom.pos.x
            self.odom_y = odom.pos.y
            self.odom_q = quaternion_from_euler(0,0,odom.pos.th,'rxyz')

    def p3dInput(self, p3d):
        #self.printBaseP3D(p3d)
        # initialize ground truth
        if self.odom_initialized == False or self.p3d_initialized == False:
            self.p3d_initialized = True
            self.p3d_xi = p3d.pos.position.x
            self.p3d_yi = p3d.pos.position.y
            self.p3d_qi =[ p3d.pos.orientation.x \
                          ,p3d.pos.orientation.y \
                          ,p3d.pos.orientation.z \
                          ,p3d.pos.orientation.w]
        else:
            # update ground truth
            self.p3d_x = p3d.pos.position.x
            self.p3d_y = p3d.pos.position.y
            self.p3d_q =[ p3d.pos.orientation.x \
                         ,p3d.pos.orientation.y \
                         ,p3d.pos.orientation.z \
                         ,p3d.pos.orientation.w]

    def bumpedInput(self, bumpString):
        print "robot touched something! ", bumpString.data
        self.bumped = True
    
    def stateInput(self, state):
        if self.publish_goal:
          state_eul = euler_from_quaternion([state.goal.pose.orientation.x,state.goal.pose.orientation.y,state.goal.pose.orientation.z,state.goal.pose.orientation.w])
          print "target: ", self.target_x, ",", self.target_y, ",", self.target_t
          print "state.goal: (", state.goal.pose.position.x, ",", state.goal.pose.position.y, ",", state.goal.pose.position.z \
                           ,",", state_eul[0], ",", state_eul[1], ",", state_eul[2] \
                           ,") status:",state.status.value, " comment:" , state.status.comment
          if abs(state.goal.pose.position.x-self.target_x)<FLOAT_TOL and \
             abs(state.goal.pose.position.y-self.target_y)<FLOAT_TOL and \
             abs(shortest_angular_distance(state_eul[2],self.target_t))<FLOAT_TOL and \
             ( state.status.value == 4 ):
            print "state goal has been published: ", state.goal.pose.position.x, ",", state.goal.pose.position.y, ",", state_eul[2]
            self.publish_goal = False
    
    def amclInput(self, amcl_pose):
        if self.publish_initialpose:
          print "/amcl_pose received, ",amcl_pose
          amcl_eul = euler_from_quaternion([amcl_pose.pose.orientation.x,amcl_pose.pose.orientation.y,amcl_pose.pose.orientation.z,amcl_pose.pose.orientation.w])
          if abs(amcl_pose.pose.position.x    - self.initialpose[0]) < AMCL_TOL and \
             abs(amcl_pose.pose.position.y    - self.initialpose[1]) < AMCL_TOL and \
             abs(amcl_eul[2]                  - self.initialpose[2]) < AMCL_TOL:
            print "initial_pose matches, stop setPose begin publishing goal."
            self.publish_initialpose = False
          else:
            print "initial_pose mismatch, continue setPose."

    def cmd_velInput(self, cmd_vel):
          print "cmd_vel: ", cmd_vel.vel.vx, ",", cmd_vel.vel.vy, ",", cmd_vel.vel.vz \
                           , cmd_vel.ang_vel.vx, ",", cmd_vel.ang_vel.vy, ",", cmd_vel.ang_vel.vz
    
    def test_set_goal(self):
        print "LNK\n"

        #pub_base = rospy.Publisher("cmd_vel", BaseVel)
        pub_goal = rospy.Publisher("/move_base/activate", PoseStamped)
        pub_pose = rospy.Publisher("initialpose", PoseWithCovariance)
        rospy.Subscriber("base_pose_ground_truth", PoseWithRatesStamped, self.p3dInput)
        rospy.Subscriber("odom"                  , RobotBase2DOdom     , self.odomInput)
        rospy.Subscriber("base_bumper/info"      , String              , self.bumpedInput)
        rospy.Subscriber("torso_lift_bumper/info", String              , self.bumpedInput)
        rospy.Subscriber("/move_base/feedback"   , MoveBaseState       , self.stateInput)
        rospy.Subscriber("/amcl_pose"            , PoseWithCovariance  , self.amclInput)

        # below only for debugging build 303, base not moving
        rospy.Subscriber("cmd_vel"               , PoseDot             , self.cmd_velInput)

        rospy.init_node(NAME, anonymous=True)

        # get goal from commandline
        print "------------------------"
        for i in range(0,len(self.args)):
          print " sys argv:", self.args[i]
          if self.args[i] == '-x':
            if len(self.args) > i+1:
              self.target_x = float(self.args[i+1])
              print "target x set to:",self.target_x
          if self.args[i] == '-y':
            if len(self.args) > i+1:
              self.target_y = float(self.args[i+1])
              print "target y set to:",self.target_y
          if self.args[i] == '-t':
            if len(self.args) > i+1:
              self.target_t = float(self.args[i+1])
              self.target_q =  quaternion_from_euler(0,0,self.target_t,'rxyz')
              print "target t set to:",self.target_t
          if self.args[i] == '-nav_t_tol':
            if len(self.args) > i+1:
              self.nav_t_tol = float(self.args[i+1])
              if self.nav_t_tol == 0:
                print "nav_t_tol check disabled"
              else:
                print "nav_t_tol set to:",self.nav_t_tol
          if self.args[i] == '-nav_xy_tol':
            if len(self.args) > i+1:
              self.nav_xy_tol = float(self.args[i+1])
              if self.nav_xy_tol == 0:
                print "nav_xy_tol check disabled"
              else:
                print "nav_xy_tol set to:",self.nav_xy_tol
          if self.args[i] == '-odom_t_tol':
            if len(self.args) > i+1:
              self.odom_t_tol = float(self.args[i+1])
              if self.odom_t_tol == 0:
                print "odom_t_tol check disabled"
              else:
                print "odom_t_tol set to:",self.odom_t_tol
          if self.args[i] == '-odom_xy_tol':
            if len(self.args) > i+1:
              self.odom_xy_tol = float(self.args[i+1])
              if self.odom_xy_tol == 0:
                print "odom_xy_tol check disabled"
              else:
                print "odom_xy_tol set to:",self.odom_xy_tol
          if self.args[i] == '-timeout':
            if len(self.args) > i+1:
              self.test_timeout = float(self.args[i+1])
              print "test_timeout set to:",self.test_timeout
          if self.args[i] == '-amcl':
            if len(self.args) > i+3:
              self.publish_initialpose = True
              self.initialpose = [float(self.args[i+1]),float(self.args[i+2]),float(self.args[i+3])]
              print "using amcl, will try to initialize pose to: ",self.initialpose
            else:
              self.publish_initialpose = False
              print "using fake localization, need 3 arguments for amcl (x,y,th)"

        print " target:", self.target_x, self.target_y, self.target_t
        print "------------------------"

        timeout_t = time.time() + self.test_timeout

        # wait for result
        while not rospy.is_shutdown() and not self.success and time.time() < timeout_t:

            #create a temp header for publishers
            h = rospy.Header();
            h.stamp = rospy.get_rostime();
            h.frame_id = "/map"
            # publish initial pose until /amcl_pose is same as intialpose
            if self.publish_initialpose:
              p = Point(self.initialpose[0], self.initialpose[1], 0)
              tmpq = quaternion_from_euler(0,0,self.initialpose[2],'rxyz')
              q = Quaternion(tmpq[0],tmpq[1],tmpq[2],tmpq[3])
              pose = Pose(p,q)
              print "publishing initialpose",h,p,COV[0]
              pub_pose.publish(PoseWithCovariance(h, pose,COV))
            else:
              # send goal until state /move_base/feedback indicates goal is received
              p = Point(self.target_x, self.target_y, 0)
              tmpq = quaternion_from_euler(0,0,self.target_t,'rxyz')
              q = Quaternion(tmpq[0],tmpq[1],tmpq[2],tmpq[3])
              pose = Pose(p,q)
              print "publishing goal",self.publish_goal
              if self.publish_goal:
                pub_goal.publish(PoseStamped(h, pose))

            time.sleep(1.0)

            # compute angular error between deltas in odom and p3d
            # compute delta in odom from initial pose
            print "========================"
            tmpoqi = quaternion_inverse(self.odom_qi)
            odom_q_delta = quaternion_multiply(tmpoqi,self.odom_q)
            print "debug odom_qi:" , self.odom_qi
            print "debug tmpoqi:" ,  tmpoqi
            print "debug odom_q_delta:" ,  odom_q_delta
            print "odom delta:" , euler_from_quaternion(odom_q_delta)
            # compute delta in p3d from initial pose
            tmppqi = quaternion_inverse(self.p3d_qi)
            p3d_q_delta = quaternion_multiply(tmppqi,self.p3d_q)
            print "p3d delta:" , euler_from_quaternion(p3d_q_delta)
            # compute delta between odom and p3d
            tmpdqi = quaternion_inverse(p3d_q_delta)
            delta = quaternion_multiply(tmpdqi,odom_q_delta)
            delta_euler = euler_from_quaternion(delta)
            odom_drift_dyaw = delta_euler[2]
            print "odom drift from p3d:" , euler_from_quaternion(delta)

            # compute delta between target and p3d
            tmptqi = quaternion_inverse(self.target_q)
            navdq = quaternion_multiply(tmptqi,self.p3d_q)
            navde = euler_from_quaternion(navdq)
            nav_dyaw = navde[2]
            print "nav euler off target:" , navde

            # check odom error (odom error from ground truth)
            odom_xy_err =  max(abs(self.odom_x - self.p3d_x - self.odom_xi + self.p3d_xi ),abs(self.odom_y - self.p3d_y - self.odom_yi + self.p3d_yi ))
            odom_t_err  =  abs(odom_drift_dyaw)

            # check total error (difference between ground truth and target)
            nav_xy_err  =  max(abs(self.p3d_x - self.target_x),abs(self.p3d_y - self.target_y))
            nav_t_err   =  abs(nav_dyaw)

            print "odom drift error: (",self.odom_x - self.p3d_x - self.odom_xi + self.p3d_xi,",",self.odom_y - self.p3d_y - self.odom_yi + self.p3d_yi,")"
            print "nav translation error: (",self.p3d_x - self.target_x,",",self.p3d_y - self.target_y,")"
            print "time remaining: ", timeout_t - time.time()

            print "nav theta error:" + str(nav_t_err) + " nav_t_tol:" + str(self.nav_t_tol)
            print "odom theta error:" + str(odom_t_err) + " odom_t_tol: " + str(self.odom_t_tol)

            self.success = True
            # check to see if collision happened
            if self.bumped == True:
                self.success = False
                print "Hit the wall."
            # check to see if nav tolerance is ok
            if self.nav_t_tol > 0 and nav_t_err > self.nav_t_tol:
                self.success = False
                print "Nav theta out of tol."
            if self.nav_xy_tol > 0 and nav_xy_err > self.nav_xy_tol:
                self.success = False
                print "Nav xy out of tol."
            # check to see if odom drift from ground truth tolerance is ok
            if self.odom_t_tol > 0 and odom_t_err > self.odom_t_tol:
                self.success = False
                print "Odom theata out of tol."
            if self.odom_xy_tol > 0 and odom_xy_err > self.odom_xy_tol:
                print "Odom xy out of tol."
                self.success = False

        self.assert_(self.success)
        
def print_usage(exit_code = 0):
    print '''Commands:
    -x <x target position>  - target x location
    -y <y target position>  - target y location
    -th <target yaw> - target robot yaw
    -nav_xy_tol <tol> - target xy location error tolerance, set to 0 to disable, default = 0.7 m
    -nav_t_tol <tol> - target yaw error tolerance, set to 0 to disable, default = 0.1 rad
    -odom_xy_tol <tol> - odom and ground truth xy drift error tolerance, set to 0 to disable, default = 0.1 m
    -odom_t_tol <tol> - odom and ground truth yaw drift error tolerance, set to 0 to disable, default = 0.1 rad
    -timeout <seconds> - test timeout in seconds. default to 50 seconds
    -amcl <x initial position> <y initial position> <initial yaw> - initial pose for amcl.  If unspecified, assume fake localization is used.
'''

if __name__ == '__main__':
    #print usage if not arguments
    if len(sys.argv) == 1:
      print_usage()
    else:
      rostest.run(PKG, sys.argv[0], NavStackTest, sys.argv) #, text_mode=True)


