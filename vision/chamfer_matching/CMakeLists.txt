cmake_minimum_required(VERSION 2.4.6)

include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
set(ROS_BUILD_TYPE RelWithDebInfo)
rospack(chamfer_matching)

rospack_add_library(chamfer_matching src/chamfer_matching.cpp)
