
cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
#set(ROS_BUILD_TYPE Release)
add_definitions(-Wall)
rospack(pr2_ik)
genmsg()
gensrv()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

rospack_add_library(pr2_ik
 src/pr2_ik.cpp
 src/pr2_ik_solver.cpp)

rospack_add_executable(pr2_ik_controller src/pr2_ik_controller_new.cpp)
target_link_libraries(pr2_ik_controller pr2_ik)

rospack_add_executable(pr2_ik_node src/pr2_ik_node.cpp)
target_link_libraries(pr2_ik_node pr2_ik)

rospack_add_executable(pr2_ik_regression_test EXCLUDE_FROM_ALL test/test_ik_regress.cpp)
rospack_declare_test(pr2_ik_regression_test)
target_link_libraries(pr2_ik_regression_test pr2_ik gtest)

rospack_add_rostest(test_pr2_ik.xml)

#rospack_add_executable(test_fk_simple test/test_fk_simple.cpp)
#target_link_libraries(test_fk_simple pr2_ik)
#rospack_add_executable(test_ik_node test/test_ik_node.cpp)
#target_link_libraries(test_ik_node pr2_ik)
#rospack_add_executable(test_ik_wc_node test/test_ik_with_collision.cpp)
#target_link_libraries(test_ik_node pr2_ik)
#rospack_add_executable(pr2_ik_single_test test/test_ik_single.cpp)
#target_link_libraries(pr2_ik_single_test pr2_ik)
#rospack_add_executable(pr2_ik_single_search_test test/test_ik_search_single.cpp)
#target_link_libraries(pr2_ik_single_search_test pr2_ik)
#rospack_add_executable(test_ik_controller test/test_ik_controller.cpp)
#target_link_libraries(test_ik_controller pr2_ik)
#rospack_add_executable(test_ik_simple test/test_ik_simple.cpp)
#target_link_libraries(test_ik_simple pr2_ik)
