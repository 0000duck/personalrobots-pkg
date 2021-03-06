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

# Author: Kevin Watts                                                           

# Moves elbow from max to min at 0.5Hz using postion controller
# Part of packet drop test.

import roslib
roslib.load_manifest('life_test')

import sys, os
from time import sleep

import rospy
from std_msgs.msg import Float64
from robot_srvs.srv import SpawnController, KillController


def xml_for():
    return "\
<controller name=\"upperarm_life_controller\" type=\"JointPositionControllerNode\">\
  <joint name=\"r_elbow_flex_joint\" >\
     <pid p=\"50\" i=\"5\" d=\"0\" iClamp=\"1.0\" \>\
  </joint>\
</controller>"

def xml_for_roll():
    return "\
<controller name=\"forearm_roll_controller\" type=\"JointEffortControllerNode\">\
  <joint name=\"r_forearm_roll_joint\" />\
</controller>"

spawn_controller = rospy.ServiceProxy('spawn_controller', SpawnController)
kill_controller = rospy.ServiceProxy('kill_controller', KillController)


def main():
    rospy.wait_for_service('spawn_controller')
    rospy.init_node('ua_life_test', anonymous=True)

    roll = False
    if rospy.get_param('forearm_roll') == 'true':
        roll = True
        mechanism.spawn_controller(xml_for_roll(), 1)
    
    spawn_controller(xml_for(), 1)

    arm_pos = rospy.Publisher('upperarm_life_controller/set_command', Float64)
    fore_roll = rospy.Publisher('forearm_roll_controller/command', Float64)

    try:
        while not rospy.is_shutdown():
            if roll:
                fore_roll.publish(Float64(1.0))
            arm_pos.publish(Float64(-0.05))
            sleep(0.5)
            arm_pos.publish(Float64(-2.00))
            sleep(0.5)
    except Exception, e:
        print "Caught exception!"
        import traceback
        traceback.print_exc()
    finally:
        if roll:
            fore_roll.publish(Float64(0.0))
            kill_controller('forearm_roll_controller')

        arm_pos.publish(Float64(0))
        kill_controller('upperarm_life_controller')


if __name__ == '__main__':
    main()
