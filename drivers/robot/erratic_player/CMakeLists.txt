cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
rospack(erratic_player)
rospack_add_executable(erratic_player erratic_player.cc)
