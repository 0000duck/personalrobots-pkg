#!/usr/bin/env python

import roslib; roslib.load_manifest('kinematic_calibration') 
import sys
import rospy
from geometry_msgs.msg import PointStamped, Point
from time import sleep
from joy.msg import Joy 

def joy_callback(data, pub):
    print 'Got joystick Comand'
    if data.buttons[7] == 1:
        pub.publish(PointStamped(roslib.msg.Header(None, None, 'r_gripper_palm_link'), Point(0.17, 0, 0)))
        print 'Sent head Command'

if __name__ == '__main__':
    rospy.init_node('joy_head_commander', anonymous=False)
    head_publisher = rospy.Publisher('head_controller/point_head', PointStamped)


    sub = rospy.Subscriber("/joy", Joy, joy_callback,
	                   head_publisher)

    print 'Ready to send joystick commands as pointhead'
    rospy.spin()
