cmake_minimum_required(VERSION 2.8.3)
project(manycal)

set(CMAKE_CXX_FLAGS "-g -Wall -std=c++11")

set(CMAKE_BUILD_TYPE Release)

find_package(catkin REQUIRED 
	COMPONENTS		camera_info_manager
					cv_bridge
					image_transport
					message_generation
					nodelet
					roscpp
					sensor_msgs
					std_msgs
					atags
					v4l2_cam
					argus_common
					argus_msgs
)
include_directories( ${catkin_INCLUDE_DIRS} )

# Use Boost for most utilities, threading
find_package(Boost REQUIRED)
include_directories( ${Boost_INCLUDE_DIRS} )

# Find OpenCV3 and set flag
find_package(OpenCV 3 REQUIRED)
add_definitions( "-DOPENCV3=1" )
include_directories( ${OpenCV_INCLUDE_DIRS} )

find_package(isam REQUIRED)
include_directories( ${isam_INCLUDE_DIRS} )

add_message_files(
	FILES			TagCorrespondence.msg 
)

generate_messages(
	DEPENDENCIES 	std_msgs
					argus_msgs
)

catkin_package(
	INCLUDE_DIRS 	include
	
	LIBRARIES 		manycal
					
	CATKIN_DEPENDS 	cv_bridge 
					image_transport 
					message_runtime 
					nodelet
					roscpp 
					sensor_msgs 
					std_msgs
					atags
					v4l2_cam
					argus_common
					argus_msgs
					
	DEPENDS 		Boost 
					OpenCV
					isam
					system_lib
)

include_directories( include )

add_library( manycal
	src/ArrayCalibrator.cpp
)

# add_dependencies( manycal ${manycal_EXPORTED_TARGETS} )
target_link_libraries( manycal
	${Boost_LIBRARIES}
	${OpenCV_LIBS}
	${catkin_LIBRARIES}
)

add_executable( corresponder_node nodes/corresponder_node.cpp )
add_dependencies( corresponder_node ${PROJECT_NAME}_gencfg )
target_link_libraries( corresponder_node 
	${catkin_LIBRARIES} 
	manycal )

## Mark executables and/or libraries for installation
install(TARGETS manycal
	ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

file(GLOB manycal_HEADERS
	"include/${PROJECT_NAME}/*.h"
	"include/${PROJECT_NAME}/*.hpp"
)
install(FILES ${manycal_HEADERS}
	DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)