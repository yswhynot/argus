cmake_minimum_required(VERSION 2.8.3)
project(odoscan)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED 
	COMPONENTS		roscpp
					std_msgs
					sensor_msgs
					geometry_msgs
					argus_utils
					lookup
					paraset
					pcl_ros
					laser_geometry
)

message( STATUS ${CMAKE_SOURCE_DIR} )
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/")

find_package(Boost REQUIRED)

find_package(Eigen3 REQUIRED)

find_package(PCL 1.7 REQUIRED)

catkin_package(
	INCLUDE_DIRS 	include

	LIBRARIES 		odoscan

	CATKIN_DEPENDS	roscpp
					sensor_msgs 
					geometry_msgs 
					std_msgs 
					message_runtime
					argus_utils
					lookup
					paraset
					pcl_ros
					laser_geometry
					
	DEPENDS 		Boost 
					PCL 
					Eigen3
)

include_directories(include)
include_directories(
	${Boost_INCLUDE_DIRS}
	${EIGEN3_INCLUDE_DIR}
	${PCL_INCLUDE_DIRS}
	${catkin_INCLUDE_DIRS}
)

set(CMAKE_CXX_FLAGS "-g -Wall -std=c++11")

add_library( odoscan
    src/ApproximateVoxelGridFilter.cpp
    src/ICPMatcher.cpp
    src/ScanFilter.cpp
	src/ScanMatcher.cpp
	src/VoxelGridFilter.cpp
)
add_dependencies( odoscan odoscan_gencfg )
target_link_libraries( odoscan
	${PCL_LIBRARIES}
	${catkin_LIBRARIES}
)

add_executable( laser_odometry_node nodes/laser_odometry_node.cpp )
add_dependencies( laser_odometry_node odoscan_gencfg )
target_link_libraries( laser_odometry_node
					   odoscan 
					   ${catkin_LIBRARIES} )

add_executable( scan_to_cloud_converter nodes/scan_to_cloud_converter.cpp )
add_dependencies( scan_to_cloud_converter odoscan_gencfg )
target_link_libraries( scan_to_cloud_converter
					   odoscan 
					   ${catkin_LIBRARIES} )

#############
## Install ##
#############

## Mark executables and/or libraries for installation
install(TARGETS odoscan
                laser_odometry_node
                scan_to_cloud_converter
	ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)
