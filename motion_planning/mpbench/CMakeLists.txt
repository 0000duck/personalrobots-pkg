cmake_minimum_required (VERSION 2.4.6)

### If you're trying to debug include directives or so, this here
### might help:
#set (CMAKE_VERBOSE_MAKEFILE ON)

#set (ROS_BUILD_TYPE Release)
set (ROS_BUILD_TYPE Debug)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
add_definitions (-Wall)
rosbuild_init ()

##################################################

LIST (APPEND MPBENCH_LIBS
      mpbench
      costmap_2d
      sunflower
      mpglue
      nepumuk
      png)

LIST (APPEND MPBENCH_SRCS
      src/setup.cpp
      src/result.cpp
      src/world.cpp
      src/gfx.cpp)

SET (CMAKE_REQUIRED_INCLUDES /usr/local/include)
INCLUDE_DIRECTORIES (/usr/local/include)
LINK_DIRECTORIES (/usr/local/lib)

INCLUDE (CheckIncludeFile)
CHECK_INCLUDE_FILE (pgm.h MPBENCH_HAVE_NETPGM)
IF (${MPBENCH_HAVE_NETPGM})
  # should probably also check for libnetpbm existance...
  MESSAGE (STATUS "found pgm.h")
  ADD_DEFINITIONS (-DMPBENCH_HAVE_NETPGM)
  LIST (APPEND MPBENCH_LIBS netpbm)
ENDIF (${MPBENCH_HAVE_NETPGM})

INCLUDE (CheckIncludeFile)
CHECK_INCLUDE_FILE (expat.h MPBENCH_HAVE_EXPAT)
IF (${MPBENCH_HAVE_EXPAT})
  MESSAGE (STATUS "found expat.h")
  ADD_DEFINITIONS (-DMPBENCH_HAVE_EXPAT)
  LIST (APPEND MPBENCH_LIBS expat)
  LIST (APPEND MPBENCH_SRCS src/parse.cpp)
ENDIF (${MPBENCH_HAVE_EXPAT})

INCLUDE (FindOpenGL)
IF (OPENGL_FOUND)
  MESSAGE (STATUS "Found OpenGL in ${OPENGL_INCLUDE_DIR}")
  MESSAGE (STATUS "  OPENGL_LIBRARIES   ${OPENGL_LIBRARIES}")
  MESSAGE (STATUS "  OPENGL_gl_LIBRARY  ${OPENGL_gl_LIBRARY}")
  MESSAGE (STATUS "  OPENGL_glu_LIBRARY ${OPENGL_glu_LIBRARY}")
  INCLUDE_DIRECTORIES (${OPENGL_INCLUDE_DIR})
  ADD_DEFINITIONS (-DMPBENCH_HAVE_OPENGL)
  LIST (APPEND MPBENCH_LIBS ${OPENGL_glu_LIBRARY} ${OPENGL_gl_LIBRARY})
ENDIF (OPENGL_FOUND)

INCLUDE (FindGLUT)
IF (GLUT_FOUND)
  MESSAGE (STATUS "Found GLUT in ${GLUT_INCLUDE_DIR}")
  MESSAGE (STATUS "  GLUT_LIBRARIES    ${GLUT_LIBRARIES}")
  MESSAGE (STATUS "  GLUT_glut_LIBRARY ${GLUT_glut_LIBRARY}")
  INCLUDE_DIRECTORIES (${GLUT_INCLUDE_DIR})
  ADD_DEFINITIONS (-DMPBENCH_HAVE_GLUT)
  LIST (APPEND MPBENCH_LIBS ${GLUT_glut_LIBRARY})
  IF (${GLUT_Xmu_LIBRARY} MATCHES NOTFOUND)
    MESSAGE (STATUS "  GLUT_Xmu_LIBRARY  not found")
  ELSE ($GLUT_Xmu_LIBRARY MATCHES NOTFOUND)
    MESSAGE (STATUS "  GLUT_Xmu_LIBRARY  ${GLUT_Xmu_LIBRARY}")
    LIST (APPEND MPBENCH_LIBS ${GLUT_Xmu_LIBRARY})
  ENDIF (${GLUT_Xmu_LIBRARY} MATCHES NOTFOUND)
  IF (${GLUT_Xi_LIBRARY} MATCHES NOTFOUND)
    MESSAGE (STATUS "  GLUT_Xi_LIBRARY   not found")
  ELSE (${GLUT_Xi_LIBRARY} MATCHES NOTFOUND)
    MESSAGE (STATUS "  GLUT_Xi_LIBRARY   ${GLUT_Xi_LIBRARY}")
    LIST (APPEND MPBENCH_LIBS ${GLUT_Xi_LIBRARY})
  ENDIF (${GLUT_Xi_LIBRARY} MATCHES NOTFOUND)
ELSE (GLUT_FOUND)
  MESSAGE (FATAL_ERROR "GLUT not found (currently this is not optional)")
ENDIF (GLUT_FOUND)

IF (WIN32)
  MESSAGE (STATUS "Detected Microsoft Windows")
  ADD_DEFINITIONS (-DWIN32)
ELSE (WIN32)
  IF (APPLE)
    MESSAGE (STATUS "Detected Mac OS X")
    ADD_DEFINITIONS (-DOSX)
    SET (EXTRA_LIBRARIES "-lobjc")
    MESSAGE (STATUS "  EXTRA_LIBRARIES: ${EXTRA_LIBRARIES}")
  ENDIF (APPLE)
 
  IF (CMAKE_SYSTEM_NAME MATCHES Linux)
    MESSAGE (STATUS "Detected Linux")
    ADD_DEFINITIONS (-DLINUX)
  ENDIF (CMAKE_SYSTEM_NAME MATCHES Linux)
 
  IF (CMAKE_SYSTEM_NAME MATCHES OpenBSD)
    MESSAGE (STATUS "Detected OpenBSD")
    ADD_DEFINITIONS (-DOPENBSD)
  ENDIF (CMAKE_SYSTEM_NAME MATCHES OpenBSD)
ENDIF(WIN32)

##################################################

set (ROS_ESTAR_DIR $ENV{ROS_ESTAR_DIR})
if (ROS_ESTAR_DIR)
  message (STATUS "ROS_ESTAR_DIR is ${ROS_ESTAR_DIR}")
  set (CMAKE_REQUIRED_INCLUDES ${ROS_ESTAR_DIR}/include)
  include_directories (${ROS_ESTAR_DIR}/include)
  link_directories (${ROS_ESTAR_DIR}/lib)
else (ROS_ESTAR_DIR)
  message (STATUS "ROS_ESTAR_DIR is not set")
endif (ROS_ESTAR_DIR)

include (CheckIncludeFileCXX)
check_include_file_cxx (estar/Facade.hpp MPBENCH_HAVE_ESTAR)
if (MPBENCH_HAVE_ESTAR)
  message (STATUS "found E*")
  add_definitions (-DMPBENCH_HAVE_ESTAR)
else (MPBENCH_HAVE_ESTAR)
  message (STATUS "E* not found (optional)")
endif (MPBENCH_HAVE_ESTAR)

##################################################

rosbuild_add_library (mpbench ${MPBENCH_SRCS})

rosbuild_add_executable (mpbenchmark src/mpbenchmark.cpp)
target_link_libraries (mpbenchmark ${MPBENCH_LIBS})

#rosbuild_add_executable (costmap2ascii src/costmap2ascii.cpp)
#target_link_libraries (costmap2ascii ${MPBENCH_LIBS})
