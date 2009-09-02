#!/usr/bin/python
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

# Author: Stuart Glaser

import roslib
import copy
import threading
import sys, os
from time import sleep

# Loads interface with the robot.
roslib.load_manifest('mechanism_bringup')
import rospy
from std_msgs.msg import *
from pr2_mechanism_msgs.srv import SpawnController, KillController, SwitchController, SwitchControllerRequest
from robot_mechanism_controllers.srv import *
from imu_node.srv import GetBoolStatus

def calibrate(controllers):
    spawn_controller = rospy.ServiceProxy('spawn_controller', SpawnController)
    kill_controller = rospy.ServiceProxy('kill_controller', KillController)
    switch_controller = rospy.ServiceProxy('switch_controller', SwitchController)

    success = True

    if type(controllers) is not list:
        controllers = [controllers]

    launched = []
    try:
        # Spawns the controllers
        for c in controllers:
            resp = spawn_controller(c)
            if resp.ok == 0:
                rospy.logerr("Failed: %s" % c)
                success = False
            else:
                launched.append(c)
        print "Launched: %s" % ', '.join(launched)

        # Starts the launched controllers
        switch_controller(launched, [], SwitchControllerRequest.BEST_EFFORT)

        # Sets up callbacks for calibration completion
        waiting_for = launched[:]
        def calibrated(msg, name):  # Somewhat not thread-safe
            if name in waiting_for:
                waiting_for.remove(name)
        for name in waiting_for:
            rospy.Subscriber("%s/calibrated" % name, Empty, calibrated, name)

        # Waits until all the controllers have calibrated
        while waiting_for and not rospy.is_shutdown():
            print "Waiting for: %s" % ', '.join(waiting_for)
            sleep(0.5)
    finally:
        [kill_controller(name) for name in launched]

    return success

def calibrate_imu():
    print "Waiting up to 20s for IMU calibration to complete."
    endtime = rospy.get_time() + 20
    try:
        rospy.wait_for_service('imu/is_calibrated', 20)
        is_calibrated = rospy.ServiceProxy('imu/is_calibrated',GetBoolStatus)
        while True:
            maxtime = max(1,endtime - rospy.get_time())
            if is_calibrated(timeout=maxtime):
                return True
            if rospy.get_time() > endtime:
                rospy.logerr("Timed out waiting for IMU calibration.")
                return False
            rospy.sleep(1)
    except:
        rospy.logerr("Wait for IMU calibration failed: %s"%sys.exc_info()[0])
        return False

def main():
    rospy.wait_for_service('spawn_controller')
    if  rospy.is_shutdown(): return

    rospy.init_node('calibration', anonymous=True)

    # Don't calibrate the IMU unless ordered to by user
    cal_imu = rospy.get_param('calibrate_imu', False)

    if cal_imu:
        imustatus = calibrate_imu()
    else: 
        imustatus = True

    xml = ''

    controllers = rospy.myargv()[1:]

    if not calibrate(controllers):
        sys.exit(3)

    if not imustatus:
        print "Mechanism calibration complete, but IMU calibration failed."
        sys.exit(2)

    print "Calibration complete"

if __name__ == '__main__':
    main()
