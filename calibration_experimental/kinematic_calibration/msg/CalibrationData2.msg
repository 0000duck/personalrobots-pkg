# This is a meta message that stores data from multiple sensors along
# with the current mechanism state of the robot.  Having this meta packet
# is extremely useful when trying to do kinematic calibration.

Header header

# Raw Stereocam Sensor Data
stereo_msgs/RawStereo raw_stereo

# Mechanism State - Stores the robots joint angles
mechanism_msgs/MechanismState mechanism_state

# PointCloud generated from the tilt laser
sensor_msgs/PointCloud laser_cloud

# Data that we captured from phasespace
mocap_msgs/MocapSnapshot mocap_snapshot

# Data from multiple monocular cameras
sensor_msgs/Image[] image
sensor_msgs/CameraInfo[] cam_info
