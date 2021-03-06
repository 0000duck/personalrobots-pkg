cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
rospack(people)
genmsg()
gensrv()
#find_library(curl REQUIRED)


rospack_add_boost_directories()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

rospack_add_executable(timing_diagnostics_node
                       src/timing_diagnostics_node.cpp )
rospack_link_boost(timing_diagnostics_node thread)

rospack_add_executable(people_tracker
                       src/filter/uniform_vector.cpp 
                       src/filter/gaussian_vector.cpp 
                       src/filter/gaussian_pos_vel.cpp 
                       src/filter/mcpdf_pos_vel.cpp
                       src/filter/mcpdf_vector.cpp
                       src/filter/sysmodel_pos_vel.cpp
                       src/filter/sysmodel_vector.cpp
                       src/filter/measmodel_pos.cpp
                       src/filter/measmodel_vector.cpp
		       src/filter/tracker_particle.cpp 
		       src/filter/tracker_kalman.cpp 
#		       src/filter/detector_particle.cpp 
		       src/filter/people_tracking_node.cpp )
rospack_link_boost(people_tracker thread)

rospack_add_executable(face_detector
                       src/face_detection.cpp 
                       src/people.cpp 
                       src/utils.cpp)
rospack_link_boost(face_detector thread)

rospack_add_executable(people_follower
                       src/people_follower.cpp )
rospack_link_boost(people_follower thread)

rospack_add_executable(stereo_face_color_tracker
                       src/stereo_face_color_tracker.cpp 
                       src/people.cpp 
                       src/utils.cpp)
rospack_link_boost(stereo_face_color_tracker thread)

rospack_add_executable(stereo_color_tracker
		      src/stereo_color_tracker.cpp)
rospack_link_boost(stereo_color_tracker thread)

rospack_add_executable(track_starter_gui
                       src/track_starter_gui.cpp 
                       src/utils.cpp)
rospack_link_boost(track_starter_gui thread)


find_package(PythonLibs)
include_directories(${PYTHON_INCLUDE_PATH})

#rospack_add_library(pyface_detector
#                    src/py.cpp src/face_detection.cpp src/people.cpp)
#set_target_properties(pyface_detection PROPERTIES OUTPUT_NAME face_detection PREFIX "")

rospack_add_executable(leg_detector 
                       src/leg_detector.cpp 
                       src/calc_leg_features.cpp
                       src/filter/tracker_kalman.cpp)

rospack_add_executable(train_leg_detector 
                       src/train_leg_detector.cpp 
                       src/calc_leg_features.cpp)

#rospack_add_pyunit(test/directed.py)

rospack_add_executable(pedestrian_detector_HOG
			src/pedestrian_detector_HOG.cpp)
