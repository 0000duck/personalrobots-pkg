cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
add_definitions(-Wall)
rospack(orrosplanning)

rospack_add_boost_directories()

rospack_add_library(orrosplanning src/orrosplanningmain.cpp)
rospack_link_boost(orrosplanning thread)
