#! /usr/bin/python
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

# Author: Kevin Watts

# Launches effort controller and drives gripper open and close at random times

import roslib
roslib.load_manifest('life_test')
import rospy
from mechanism_control import mechanism
from std_msgs.msg import Float64
from mechanism_msgs.srv import SpawnController, KillController

import random
import sys

import time

def xml_for(control_name, joint):
    return "\
<controller name=\"%s\" type=\"JointEffortControllerNode\">\
<joint name=\"%s\" />\
</controller>" % (control_name, joint)

def main():
    joint = sys.argv[1]

    control_name = joint + '_controller'

    rospy.init_node('gripper_life_' + control_name, anonymous=True)
    rospy.wait_for_service('spawn_controller')
    spawn_controller = rospy.ServiceProxy('spawn_controller', SpawnController)
    kill_controller = rospy.ServiceProxy('kill_controller', KillController)

    eff = 100

    try:
        resp = spawn_controller(xml_for(control_name, joint), 1)
        if len(resp.ok) < 1 or not resp.ok[0]:
            rospy.logerr("Failed to spawn effort controller %s" % joint)
            rospy.logerr(resp.error[0])
            sys.exit(1)

        control_topic = '%s/command/' % control_name
        pub = rospy.Publisher(control_topic, Float64)

        while not rospy.is_shutdown():
            time.sleep(random.uniform(0.5, 2.0))
            m = Float64(eff)
            
            eff = eff * -1
            pub.publish(m)
    except:
        import traceback
        traceback.print_exc()
    finally:
        kill_controller(control_name)

if __name__ == '__main__':
    main()
