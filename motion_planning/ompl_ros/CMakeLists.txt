cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
rospack(ompl_ros)

set(ROS_BUILD_TYPE Debug)

rospack_add_boost_directories()

rospack_add_library(ompl_ros src/ModelBase.cpp
			     src/ModelKinematic.cpp
			     src/ModelDynamic.cpp
			     src/base/ProjectionEvaluators.cpp
			     src/base/GoalDefinitions.cpp
			     src/kinematic/SpaceInformation.cpp	
			     src/kinematic/StateValidator.cpp
			     src/dynamic/SpaceInformation.cpp	
			     src/dynamic/StateValidator.cpp
			     src/dynamic/ForwardPropagationModels.cpp)

rospack_link_boost(ompl_ros thread)
