cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
rospack(resource_locator)

rospack_add_boost_directories()

rospack_add_library(resource_locator src/reslocator.cpp)
rospack_link_boost(resource_locator regex)
