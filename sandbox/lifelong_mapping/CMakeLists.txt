cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

# Set the build type.  Options are:
#  Coverage       : w/ debug symbols, w/o optimization, w/ code-coverage
#  Debug          : w/ debug symbols, w/o optimization
#  Release        : w/o debug symbols, w/ optimization
#  RelWithDebInfo : w/ debug symbols, w/ optimization
#  MinSizeRel     : w/o debug symbols, w/ optimization, stripped binaries
#set(ROS_BUILD_TYPE RelWithDebInfo)

rospack(lifelong_mapping)

#set the default path for built executables to the "bin" directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#set the default path for built libraries to the "lib" directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

genmsg()
gensrv()

rospack_add_library(${PROJECT_NAME} src/database.cpp src/pose_graph.cpp src/graph_server.cpp src/graph_client.cpp src/database_publisher.cpp)
target_link_libraries(${PROJECT_NAME} db_cxx)

rospack_add_executable(graph_server src/graph_server_node.cpp)
target_link_libraries(graph_server ${PROJECT_NAME})

rospack_add_executable(db_test test/db_test.cpp)
target_link_libraries(db_test ${PROJECT_NAME})

rospack_add_executable(query_test test/query_test.cpp)
target_link_libraries(query_test ${PROJECT_NAME})

rospack_add_executable(data_topic_test test/data_topic_test.cpp)
target_link_libraries(data_topic_test ${PROJECT_NAME})

#common commands for building c++ executables and libraries
#rospack_add_library(${PROJECT_NAME} src/example.cpp)
#target_link_libraries(${PROJECT_NAME} another_library)
#rospack_add_boost_directories()
#rospack_link_boost(${PROJECT_NAME} thread)
#rospack_add_executable(example examples/example.cpp)
#target_link_libraries(example ${PROJECT_NAME})
