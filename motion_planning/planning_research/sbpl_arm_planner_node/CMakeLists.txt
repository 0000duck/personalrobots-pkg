cmake_minimum_required(VERSION 2.4.6)
set(ROS_BUILD_TYPE debug)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
rospack(sbpl_arm_planner_node)
# gensrv()

# Build executables in the bin directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

rospack_add_executable(sbpl_arm_planner_node src/sbpl_arm_planner_node.cpp)
target_link_libraries(sbpl_arm_planner_node pm_wrapper)

# rospack_add_executable(test_fk test/test_fk.cpp)
# rospack_add_executable(sbpl_dh_regression_test test/test_dh_regress.cpp)

# rospack_declare_test(sbpl_dh_regression_test)
# target_link_libraries(sbpl_dh_regression_test gtest) # Run 'make test'

#rospack_add_executable(test_fk_simple test/test_fk_simple.cpp)
#target_link_libraries(test_fk_simple pr2_ik)

# rospack_add_rostest(test_pr2_ik.xml)
