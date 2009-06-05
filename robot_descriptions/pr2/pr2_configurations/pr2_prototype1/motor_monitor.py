#!/usr/bin/env python

import roslib
roslib.load_manifest('2dnav_pr2')

import sys, traceback, logging, rospy, os
from diagnostic_msgs.msg import DiagnosticMessage, DiagnosticStatus, DiagnosticValue, DiagnosticString


NAME = 'motor_monitor'

def callback(data):
  for msg in data.status:
    if msg.name == "EtherCAT Master":
      if msg.message == "Motors halted":
        file = open('reset_motors.log', 'a')
        log_msg = "Resetting motors at time: %d.%d\n" % (data.header.stamp.secs, data.header.stamp.nsecs)
        file.write(log_msg)
        file.close()
        os.system('rosrun pr2_etherCAT reset_motors.py')
        print "Resetting motors"

def listener_with_user_data():
    rospy.Subscriber("/diagnostics", DiagnosticMessage, callback)
    rospy.init_node(NAME, anonymous=True)
    rospy.spin()

if __name__ == '__main__':
    try:
        listener_with_user_data()
    except KeyboardInterrupt, e:
        pass
    print "exiting"
