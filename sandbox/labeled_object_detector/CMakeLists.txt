cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

# Set the build type.  Options are:
#  Coverage       : w/ debug symbols, w/o optimization, w/ code-coverage
#  Debug          : w/ debug symbols, w/o optimization
#  Release        : w/o debug symbols, w/ optimization
#  RelWithDebInfo : w/ debug symbols, w/ optimization
#  MinSizeRel     : w/o debug symbols, w/ optimization, stripped binaries
set(ROS_BUILD_TYPE Debug)

rospack(labeled_object_detector)

#set the default path for built executables to the "bin" directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#set the default path for built libraries to the "lib" directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#uncomment if you have defined messages
genmsg()
#uncomment if you have defined services
#gensrv()

#common commands for building c++ executables and libraries
rospack_add_library(${PROJECT_NAME} src/pcd_misc.cpp src/planar_object_detector.cpp src/object_model.cpp)


#target_link_libraries(${PROJECT_NAME} another_library)
#rospack_add_boost_directories()
#rospack_link_boost(${PROJECT_NAME} thread)
#rospack_add_executable(example examples/example.cpp)
#target_link_libraries(example ${PROJECT_NAME})

rospack_add_executable(planar_object_detector  src/planar_object_detector_node.cpp)
target_link_libraries(planar_object_detector ${PROJECT_NAME})

rospack_add_executable(detect_nearest_object_action  src/detect_nearest_object_action.cpp)
target_link_libraries(detect_nearest_object_action ${PROJECT_NAME})

rospack_add_executable(test_nearest_action test/test_nearest_action.cpp)
target_link_libraries(test_nearest_action ${PROJECT_NAME})

rospack_add_executable(test_nearest_action2 test/test_nearest_action2.cpp)
target_link_libraries(test_nearest_action2 ${PROJECT_NAME})


#rospack_add_gtest(test_pcd_segmentation test/test_pcd_segmentation.cpp)
#target_link_libraries(test_pcd_segmentation ${PROJECT_NAME})
