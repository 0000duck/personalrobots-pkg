cmake_minimum_required(VERSION 2.4.6)

include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
set(ROS_BUILD_TYPE RelWithDebInfo)
rospack(plugs_core)
gensrv()
genmsg()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)


rospack_add_executable(run_detect_outlet_fine
  src/run_detect_outlet_fine.cpp
  src/action_detect_outlet_fine.cpp
  )

rospack_add_executable(run_detect_outlet_coarse
  src/run_detect_outlet_coarse.cpp
  src/action_detect_outlet_coarse.cpp
  )

rospack_add_executable(run_plugs_untuck_arms
  src/run_untuck_arms.cpp
  src/action_untuck_arms.cpp
  )

rospack_add_executable(run_move_and_grasp_plug
  src/run_move_and_grasp_plug.cpp
  src/action_move_and_grasp_plug.cpp
  )

rospack_add_executable(run_localize_plug_in_gripper
  src/run_localize_plug_in_gripper.cpp
  src/action_localize_plug_in_gripper.cpp
  )

rospack_add_executable(run_plug_in
  src/run_plug_in.cpp
  src/action_plug_in.cpp
  )

rospack_add_executable(run_plug_in2
  src/run_plug_in2.cpp
  src/action_plug_in2.cpp
  )

rospack_add_executable(run_unplug
  src/run_unplug.cpp
  src/action_unplug.cpp
  )

rospack_add_executable(run_stow_plug
  src/run_stow_plug.cpp
  src/action_stow_plug.cpp
  )



# test

rospack_add_executable(test_executive test/test_executive.cpp)
rospack_add_executable(test_executive_outlet_coarse test/test_executive_outlet_coarse.cpp)
rospack_add_executable(test_executive_hybrid test/test_executive_hybrid.cpp)

rospack_add_executable(test_approach test/test_approach.cpp)
rospack_add_executable(test_state_stow test/test_stage_stow.cpp)
rospack_add_executable(test_plug_in test/test_plug_in.cpp)


# check for OpenMP
include(CheckIncludeFile)
include(CheckCXXCompilerFlag)
include(CheckLibraryExists)

if( WIN32 )
  CHECK_INCLUDE_FILE(omp.h HAVE_OMP_H)
  if( HAVE_OMP_H )
    message(STATUS "Using OpenMP")
    check_cxx_compiler_flag(/openmp HAVE_OPENMP)

    if( HAVE_OPENMP )
      add_definitions("/openmp")
    endif( HAVE_OPENMP )
  endif( HAVE_OMP_H )
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # check if compilers supports -fopenmp
  check_cxx_compiler_flag(-fopenmp HAVE_OPENMP)
  check_library_exists(gomp omp_get_num_threads "" HAS_GOMP_LIB)

  if( HAVE_OPENMP AND HAS_GOMP_LIB )
    add_definitions("-fopenmp")
    set(OPENMP_LFLAGS "-lgomp")
  endif( HAVE_OPENMP AND HAS_GOMP_LIB )
endif( WIN32 )
