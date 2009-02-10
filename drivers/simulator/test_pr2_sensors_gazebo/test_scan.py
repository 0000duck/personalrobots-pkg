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

## Gazebo test cameras validation 

PKG = 'test_pr2_sensors_gazebo'
NAME = 'test_scan'

import math
import roslib
roslib.load_manifest(PKG)
roslib.load_manifest('rostest')


import sys, unittest
import os, os.path, threading, time
import rospy, rostest
from laser_scan.msg import *

TEST_DURATION  = 15
ERROR_TOL      = 0.05
FAIL_COUNT_TOL = 10

TARGET_RANGES = [
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 2.05170464516, 2.04536008835, 2.03909778595, 2.03291654587, 2.02681565285, 
2.02079415321, 2.01485085487, 2.00898480415, 2.0031952858, 1.99748134613, 1.99184203148, 1.98627638817, 
1.98078382015, 1.97536325455, 1.9700139761, 1.96473526955, 1.95952606201, 1.95438587666, 1.94931387901, 
1.94430923462, 1.93937122822, 1.93449926376, 1.92969250679, 1.92495036125, 1.92027211189, 1.91565704346, 
1.91110467911, 1.90661418438, 1.90218496323, 1.91215276718, 1.93008923531, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 2.64686512947, 2.6173157692, 2.59563803673, 2.57963132858, 2.5657658577, 
2.55445694923, 2.54415798187, 2.53618431091, 2.52831435204, 2.5224199295, 2.51713490486, 2.51194572449, 
2.50885725021, 2.50583004951, 2.50296163559, 2.50192046165, 2.50093340874, 2.5, 2.50093340874, 
2.50192022324, 2.50296139717, 2.50582957268, 2.50885748863, 2.51194500923, 2.51713514328, 2.52241945267, 
2.52831435204, 2.53618454933, 2.54415798187, 2.55445718765, 2.5657658577, 2.57963109016, 2.59563827515, 
2.6173157692, 2.64686512947, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 2.74098372459, 2.72118186951, 2.72313261032, 2.72514390945, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 1.85238564014, 1.80339097977, 1.77707922459, 1.75727534294, 1.74103367329, 
1.72714829445, 1.71498286724, 1.70415234566, 1.69440209866, 1.68555378914, 1.67747652531, 1.67007064819, 
1.66325867176, 1.6569788456, 1.65118086338, 1.64582335949, 1.64087188244, 1.63629746437, 1.63207530975, 
1.62818443775, 1.62460672855, 1.62132656574, 1.61833024025, 1.61560595036, 1.61314368248, 1.61093437672, 
1.60897052288, 1.60724556446, 1.6057536602, 1.60449028015, 1.60345125198, 1.60263371468, 1.60203504562, 
1.60165333748, 1.60148763657, 1.60153746605, 1.60180282593, 1.60228455067, 1.60298418999, 1.60390377045, 
1.60504591465, 1.60641443729, 1.60801339149, 1.60984802246, 1.61192440987, 1.61424958706, 1.61683177948, 
1.61968040466, 1.62280631065, 1.62622225285, 1.62994265556, 1.6339840889, 1.63836622238, 1.64311158657, 
1.64824676514, 1.65380322933, 1.65981829166, 1.66633713245, 1.6734149456, 1.68112039566, 1.68954002857, 
1.69878637791, 1.709010005, 1.72041964531, 1.73332083225, 1.74819290638, 1.76587283611, 1.7881039381, 
1.82019233704, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, ]

TARGET_RANGES_NO_CUP = [
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 2.04787278175, 2.04592037201, 2.03293919563, 2.0309150219, 2.02612328529, 2.02249693871, 
2.01050376892, 1.9971985817, 1.99220633507, 1.99093174934, 1.98233509064, 1.96967220306, 1.96407520771, 
1.95673263073, 1.9632999897, 1.9541169405, 1.94116449356, 1.93745577335, 1.93260037899, 1.91873133183, 
1.92071986198, 1.92447936535, 1.91405880451, 1.90388011932, 1.90442991257, 1.92065799236, 1.94389533997, 
10.0500001907, 10.0500001907, 10.0500001907, 2.20284819603, 2.11506819725, 2.07276725769, 2.05478954315, 
2.03410387039, 2.01044726372, 1.99864196777, 1.97362887859, 1.95649659634, 1.94725096226, 1.93579721451, 
1.92892599106, 1.9206674099, 1.89596438408, 1.89780378342, 1.89246237278, 1.8796428442, 1.87438380718, 
1.85257148743, 1.8596534729, 1.84719610214, 1.83455812931, 1.85068559647, 1.83815693855, 1.83944654465, 
1.83141326904, 1.83223962784, 1.83295285702, 1.81912827492, 1.82103550434, 1.81053507328, 1.8073772192, 
1.81735467911, 1.81044983864, 1.80440604687, 1.80720567703, 1.79462218285, 1.8113617897, 1.80637574196, 
1.81443309784, 1.7996609211, 1.80886435509, 1.80233979225, 1.81417393684, 1.81837320328, 1.81973767281, 
1.81899499893, 1.81897163391, 1.81569230556, 1.81440281868, 1.82588279247, 1.82763755322, 1.8355230093, 
1.82483589649, 1.8410063982, 1.84740209579, 1.85418522358, 1.85382521152, 1.86047744751, 1.87290942669, 
1.87637448311, 1.87997066975, 1.88822221756, 1.90849196911, 1.90068280697, 1.92333364487, 1.92135071754, 
1.9316290617, 1.94617557526, 1.96299791336, 1.98042583466, 1.9886174202, 2.01968574524, 2.04413557053, 
2.06229710579, 2.09267020226, 2.14132356644, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
1.80345368385, 1.7794598341, 1.7536636591, 1.74016606808, 1.72928035259, 1.71435511112, 1.68924510479, 
1.67889750004, 1.66685688496, 1.66631317139, 1.66503357887, 1.65074276924, 1.64622533321, 1.64321935177, 
1.64316165447, 1.62697017193, 1.62436592579, 1.62134301662, 1.62517416477, 1.61807000637, 1.62088704109, 
1.61336827278, 1.5963088274, 1.6035785675, 1.59667503834, 1.60347390175, 1.59573459625, 1.5959597826, 
1.60779702663, 1.59334671497, 1.60121428967, 1.60183036327, 1.59678864479, 1.60666680336, 1.60501253605, 
1.60348498821, 1.61345434189, 1.61245715618, 1.62518751621, 1.62398457527, 1.62988734245, 1.62476825714, 
1.64009869099, 1.63341450691, 1.64568448067, 1.64412105083, 1.65664947033, 1.66021370888, 1.67184150219, 
1.68355834484, 1.69032812119, 1.70137858391, 1.7188808918, 1.72428512573, 1.74748575687, 1.77116715908, 
1.7868065834, 1.84455692768, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 10.0500001907, 
10.0500001907, 10.0500001907, 10.0500001907, ]





class PointCloudTest(unittest.TestCase):
    def __init__(self, *args):
        super(PointCloudTest, self).__init__(*args)
        self.success = False


    def printPointCloud(self, cloud):
        print "["
        i = 0
        for pt in cloud.ranges:
            sys.stdout.write(str(pt) + ", ")
            i = i + 1
            if ((i % 7) == 0):
                print "" #newline
        print "]"


    def pointInput(self, cloud):
        i = 0
        fail_count = 0
        print "Input laser scan received"
        self.printPointCloud(cloud)  #uncomment to capture new data
        while (i < len(cloud.ranges) and i < len(TARGET_RANGES_NO_CUP)):
            d = cloud.ranges[i] - TARGET_RANGES_NO_CUP[i]
            if ((d < - ERROR_TOL) or (d > ERROR_TOL)):
                fail_count += 1
                print "fail_count:" + str(fail_count) + " failed. error:" + str(d) + " exceeded tolerance:" + str(ERROR_TOL)
            i = i + 1

        if fail_count > FAIL_COUNT_TOL:
            print "Fail count too large (" + str(fail_count) + "), failing scan"
            return

        self.success = True
    
    def test_scan(self):
        print "LNK\n"
        rospy.subscribe_topic("base_scan", LaserScan, self.pointInput)
        rospy.init_node(NAME, anonymous=True)
        timeout_t = time.time() + TEST_DURATION
        while not rospy.is_shutdown() and not self.success and time.time() < timeout_t:
            time.sleep(0.1)
        self.assert_(self.success)
        
    


if __name__ == '__main__':
    rostest.run(PKG, sys.argv[0], PointCloudTest, sys.argv) #, text_mode=True)


