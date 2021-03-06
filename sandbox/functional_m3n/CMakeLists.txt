cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

rospack(functional_m3n)

#set the default path for built executables to the "bin" directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#set the default path for built libraries to the "lib" directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

rospack_add_boost_directories()

# ---[ Functional Max-Margin Markov Network library
rospack_add_library (${PROJECT_NAME}
                        src/inference_tools/vk_submodular_energy_min.cpp
			src/inference_tools/bgl_submodular_energy_min.cpp
			src/random_field.cpp
			src/m3n_model.cpp
			src/m3n_model_inference.cpp
			src/m3n_model_learning.cpp
			src/regressors/regression_tree_wrapper.cpp
			src/m3n_params.cpp
                      )
rospack_add_openmp_flags (${PROJECT_NAME})

