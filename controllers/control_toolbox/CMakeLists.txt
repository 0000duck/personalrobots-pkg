cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
include_directories(include/control_toolbox/eigen2)
rosbuild_init()
rosbuild_genmsg()
rosbuild_gensrv()
rosbuild_add_library(control_toolbox
 src/pid.cpp
 src/pid_gains_setter.cpp
 src/sine_sweep.cpp
 src/ramp_output.cpp
 src/base_position_pid.cpp
 src/dither.cpp
 src/sinusoid.cpp
 )
 
# rospack_add_executable(test_linear test/linear.cpp)
