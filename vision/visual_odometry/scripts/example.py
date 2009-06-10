import roslib
roslib.update_path('visual_odometry')
import rospy

import math
import sys
import time
import Image

sys.path.append('lib')

from stereo_utils import camera
from stereo_utils.stereo import SparseStereoFrame
from stereo_utils.descriptor_schemes import DescriptorSchemeCalonder, DescriptorSchemeSAD
from stereo_utils.feature_detectors import FeatureDetectorFast, FeatureDetector4x4, FeatureDetectorStar, FeatureDetectorHarris
from visual_odometry.visualodometer import VisualOdometer, Pose, from_xyz_euler

Fx = 100   # Focal length x
Fy = 100   # Focal length y
Tx = 0.090 # baseline in M
Clx = 320  # Center left image
Crx = 320  # Center right image
Cy = 240   # Center Y (both images)

# Camera
cam = camera.Camera((Fx, Fy, Tx, Clx, Crx, Cy))

# Feature Detector
fd = FeatureDetectorFast(300)

# Descriptor Scheme
ds = DescriptorSchemeCalonder()

# Visual Odometer
vo = VisualOdometer(cam)

for i in range(1000):
  # Left image
  lim = Image.open("%s/%06dL.png" % (sys.argv[1], i))

  # Right image
  rim = Image.open("%s/%06dR.png" % (sys.argv[1], i))

  # Combine the images to make a stereo frame
  frame = SparseStereoFrame(lim, rim, feature_detector = fd, descriptor_scheme = ds)

  # Supply the stereo frame to the visual odometer
  pose = vo.handle_frame(frame)
  print i, pose
  print

fd.summarize_timers()
ds.summarize_timers()
vo.summarize_timers()
