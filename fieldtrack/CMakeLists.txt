cmake_minimum_required(VERSION 2.8.3)
project(fieldtrack)

set(CMAKE_CXX_FLAGS "-g -Wall -std=c++11")

if (NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif ()
message(STATUS "Build configuration: ${CMAKE_BUILD_TYPE}")

# Finding dependencies
# ====================
find_package( catkin REQUIRED 
              COMPONENTS message_generation
                         roscpp
                         std_msgs 
                         nav_msgs
                         argus_utils
                         extrinsics_array
                         lookup
)

# Use Boost for most utilities, threading
find_package( Boost REQUIRED )

# Use Eigen for matrices, linear algebra
find_package( Eigen3 REQUIRED )

# Messages and services
# =====================
add_service_files( FILES ResetFilter.srv 
)

add_message_files( FILES TargetState.msg 
)

generate_messages( DEPENDENCIES std_msgs
                                geometry_msgs )

# Package configuration
# =====================
catkin_package( INCLUDE_DIRS   include

                LIBRARIES      fieldtrack

                CATKIN_DEPENDS message_runtime 
                               roscpp
                               std_msgs
                               nav_msgs
                               argus_utils
                               extrinsics_array
                               lookup

                DEPENDS        Boost
                               system_lib
)

include_directories( include
                     ${Boost_INCLUDE_DIRS}
                     ${EIGEN3_INCLUDE_DIR}
                     ${catkin_INCLUDE_DIRS}
)

add_library( fieldtrack
    src/AdaptiveCovarianceEstimator.cpp
	#src/BodyFrameTracker.cpp
	src/FieldtrackCommon.cpp
	src/FilterInfo.cpp
	#src/FixedFrameTracker.cpp
	src/ObservationSourceManager.cpp
	src/PoseDerivativeFilter.cpp
	src/StateEstimator.cpp
	src/TargetInfoManager.cpp
	#src/VelocityIntegrator.cpp
)
target_link_libraries( fieldtrack
	${Boost_LIBRARIES}
	${catkin_LIBRARIES}
)
add_dependencies( fieldtrack ${catkin_EXPORTED_TARGETS} fieldtrack_gencfg )

add_executable( state_estimator_node
	nodes/state_estimator_node.cpp )
add_dependencies( state_estimator_node ${PROJECT_NAME}_gencfg )
target_link_libraries( state_estimator_node
	fieldtrack
	${catkin_LIBRARIES} )

add_executable( odometry_converter_node
	nodes/odometry_converter_node.cpp )
add_dependencies( odometry_converter_node ${PROJECT_NAME}_gencfg )
target_link_libraries( odometry_converter_node
	fieldtrack
	${catkin_LIBRARIES} )

add_executable( velocity_converter_node
  nodes/velocity_converter_node.cpp )
add_dependencies( velocity_converter_node ${ROJECT_NAME}_gencfg )
target_link_libraries( velocity_converter_node
  fieldtrack
  ${catkin_LIBRARIES} )

add_executable( odometry_differentiator_node
	nodes/odometry_differentiator.cpp )
add_dependencies( odometry_differentiator_node ${ROJECT_NAME}_gencfg )
target_link_libraries( odometry_differentiator_node
	fieldtrack
	${catkin_LIBRARIES} )

## Mark executables and/or libraries for installation
install(TARGETS state_estimator_node
                odometry_converter_node
                odometry_differentiator_node
	ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY include/${PROJECT_NAME}/
	DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
	FILES_MATCHING PATTERN "*.h"
)
