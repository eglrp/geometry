#
# CMake file for the geometry code
#
# ------------
#
# This file compiles all programs present in the geometry repo.
# To run, perform the following from the root of the repository:
#
#	$> mkdir build
#	$> cd build
#	$> cmake ..
#	$> make
#
# Executables for each program will be stored in the bin/ directory
#
# ------------
#
# Written by Eric Turner
# July 29, 2015
#

# Set up meta information about this cmake build.
#	We require at least version 2.6 of CMake
#	This project is called geometry
cmake_minimum_required(VERSION 2.6)
project(Geometry)

# Add places to the CMAKE_MODULE_PATH
#
# Adds directory where Findxxx.cmake files reside.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

# Set the build type
# Set to RELEASE for release mode and DEBUG for debug mode
IF(NOT CMAKE_BUILD_TYPE)
	MESSAGE(STATUS "Defaulting to building targets in RELEASE mode")
	set(CMAKE_BUILD_TYPE RELEASE)
ENDIF(NOT CMAKE_BUILD_TYPE)

# Setup where we want to build the output binaries
#
# This tells CMAKE where to instruct the compiler to build the
# output executables
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin/)

# set required c++ version
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O2 -W -Wall -Wextra")

# --------------------------------------------
# -------- Find External Libraries -----------
# --------------------------------------------

#
# Required libraries:
#
#	boost opencv opencv3 xerces-c-dev eigen
#

# Find Boost
#
# This is used for both the kdtree and the threading code
find_package(Boost 1.55.0 COMPONENTS thread chrono system filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# Find opencv
#
# Used for image reading and feature tracking
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

# Find xerces
#
# Used by libe57 for generating .e57 scan files
find_package( XercesC REQUIRED )
include_directories( ${XercesC_INCLUDE_DIR} )

# Include eigen3
include_directories(/usr/local/include/eigen3/)
include_directories(/usr/include/eigen3/)

# -------------------------------------------------
# -------- Create Common Static Library -----------
# -------------------------------------------------

# Add the source directories to the search path
# 
# This makes it so that our compiler can find all the header files
# in the src/cpp directory and the z_estimation includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/include/)

# Grab all of the common source and complie it into a library
file(GLOB_RECURSE COMMON_SRC 
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/config/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/geometry/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/image/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/include/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/carve/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/conf/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/data/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/hia/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/images/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/latex/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/levels/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/mesh/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/octree/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/pointcloud_writer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/writer/OBJ*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/writer/XYZ*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/writer/PTS*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/writer/PCD*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/writer/PointCloudWriter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/reader/OBJ*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/reader/XYZ*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/reader/PTS*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/pointcloud/reader/PointCloudReader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/scanorama/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/mesh/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/timestamp/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/util/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/xmlreader/*.cpp
	)
add_library(core OBJECT ${COMMON_SRC})

# ---------------------------------------------------
# -------- Prepare Individual Executables -----------
# ---------------------------------------------------

# ------ pointcloud_gen ------
MESSAGE(STATUS "Including binary \"pointcloud_gen\"")
file(GLOB_RECURSE POINTCLOUD_GEN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/pointcloud_generation/*.cpp)
add_executable(pointcloud_gen ${POINTCLOUD_GEN_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(pointcloud_gen ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ partition_pointcloud_levels ------
MESSAGE(STATUS "Including binary \"partition_pointcloud_levels\"")
file(GLOB_RECURSE PARTITION_POINTCLOUD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/partition_pointcloud_levels/src/*.cpp)
add_executable(partition_pointcloud_levels ${PARTITION_POINTCLOUD_SRC})

# ------ xyz2dq ------
MESSAGE(STATUS "Including binary \"xyz2dq\"")
file(GLOB_RECURSE XYZ2DQ_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/xyz2dq/src/*.cpp)
add_executable(xyz2dq ${XYZ2DQ_SRC})

# ------ floorplan_gen ------
MESSAGE(STATUS "Including binary \"floorplan_gen\"")
file(GLOB_RECURSE FLOORPLAN_GEN_SRC 
	${CMAKE_CURRENT_SOURCE_DIR}/execs/dq_floorplan/src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/util/range_list.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/util/rotLib.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/carve/noisypath_io.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/config/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/xmlreader/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/geometry/system_path.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/geometry/transform.cpp
	)
add_executable(floorplan_gen ${FLOORPLAN_GEN_SRC})

# ------ fp2model ------
MESSAGE(STATUS "Including binary \"fp2model\"")
file(GLOB_RECURSE FP2MODEL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/fp2model/src/*.cpp)
add_executable(fp2model ${FP2MODEL_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(fp2model ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ filter_urg_scans ------
MESSAGE(STATUS "Including binary \"filter_urg_scans\"")
file(GLOB_RECURSE FILTER_URG_SCANS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/filter_urg_scans/*.cpp)
add_executable(filter_urg_scans ${FILTER_URG_SCANS_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(filter_urg_scans ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ filter_tango_scans ------
MESSAGE(STATUS "Including binary \"filter_tango_scans\"")
file(GLOB_RECURSE FILTER_TANGO_SCANS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/filter_tango_scans/src/*.cpp)
add_executable(filter_tango_scans ${FILTER_TANGO_SCANS_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(filter_tango_scans ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ generate_noisypath ------
MESSAGE(STATUS "Including binary \"generate_noisypath\"")
file(GLOB_RECURSE GENERATE_NOISYPATH_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/generate_noisypath/src/*.cpp)
add_executable(generate_noisypath ${GENERATE_NOISYPATH_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(generate_noisypath ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ wedge_gen ------
MESSAGE(STATUS "Including binary \"wedge_gen\"")
file(GLOB_RECURSE WEDGE_GEN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/wedge_generator/src/*.cpp)
add_executable(wedge_gen ${WEDGE_GEN_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(wedge_gen ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ scan_chunker ------
MESSAGE(STATUS "Including binary \"scan_chunker\"")
file(GLOB_RECURSE SCAN_CHUNKER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/scan_chunker/src/*.cpp)
add_executable(scan_chunker ${SCAN_CHUNKER_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(scan_chunker ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ procarve ------
MESSAGE(STATUS "Including binary \"procarve\"")
file(GLOB_RECURSE PROCARVE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/procarve/src/*.cpp)
add_executable(procarve ${PROCARVE_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(procarve ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ oct2dq ------
MESSAGE(STATUS "Including binary \"oct2dq\"")
file(GLOB_RECURSE OCT2DQ_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/oct2dq/src/*.cpp)
add_executable(oct2dq ${OCT2DQ_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(oct2dq ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ generate_hia ------
MESSAGE(STATUS "Including binary \"generate_hia\"")
file(GLOB_RECURSE GENERATE_HIA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/generate_hia/src/*.cpp)
add_executable(generate_hia ${GENERATE_HIA_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(generate_hia ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ fp_optimizer ------
MESSAGE(STATUS "Including binary \"fp_optimizer\"")
file(GLOB_RECURSE FP_OPTIMIZER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/fp_optimizer/src/*.cpp)
add_executable(fp_optimizer ${FP_OPTIMIZER_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(fp_optimizer ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ merge_fp_oct ------
MESSAGE(STATUS "Including binary \"merge_fp_oct\"")
file(GLOB_RECURSE MERGE_FP_OCT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/merge_fp_oct/src/*.cpp)
add_executable(merge_fp_oct ${MERGE_FP_OCT_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(merge_fp_oct ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ octsurf ------
MESSAGE(STATUS "Including binary \"octsurf\"")
file(GLOB_RECURSE OCTSURF_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/octsurf/src/*.cpp)
add_executable(octsurf ${OCTSURF_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(octsurf ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ rectify_images ------
MESSAGE(STATUS "Including binary \"rectify_images\"")
file(GLOB_RECURSE RECTIFY_IMAGES_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/rectify_images/src/*.cpp)
# TODO
#add_executable(rectify_images ${RECTIFY_IMAGES_SRC} $<TARGET_OBJECTS:core>)
#target_link_libraries(rectify_images ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ mesh2image ------
MESSAGE(STATUS "Including binary \"mesh2image\"")
file(GLOB_RECURSE MESH2IMAGE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/mesh2image/src/*.cpp)
# TODO
#add_executable(mesh2image ${MESH2IMAGE_SRC} $<TARGET_OBJECTS:core>)
#target_link_libraries(mesh2image ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ generate_imap ------
MESSAGE(STATUS "Including binary \"generate_imap\"")
file(GLOB_RECURSE GENERATE_IMAP_SRC 
	${CMAKE_CURRENT_SOURCE_DIR}/execs/generate_imap/src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/util/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/data/mcd/Mcdfile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/mesh/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/io/images/*.cpp
	)
add_executable(generate_imap ${GENERATE_IMAP_SRC})
target_link_libraries(generate_imap ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ generate_scanorama ------
MESSAGE(STATUS "Including binary \"generate_scanorama\"")
file(GLOB_RECURSE GENERATE_SCANORAMA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/generate_scanorama/src/*.cpp)
add_executable(generate_scanorama ${GENERATE_SCANORAMA_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(generate_scanorama ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ generate_tex ------
MESSAGE(STATUS "Including binary \"generate_tex\"")
file(GLOB_RECURSE GENERATE_TEX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/generate_tex/src/*.cpp)
add_executable(generate_tex ${GENERATE_TEX_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(generate_tex ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ wedge2xyz ------
MESSAGE(STATUS "Including binary \"wedge2xyz\"")
file(GLOB_RECURSE WEDGE2XYZ_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/wedge2xyz/src/*.cpp)
add_executable(wedge2xyz ${WEDGE2XYZ_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(wedge2xyz ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ generate_msd ------
MESSAGE(STATUS "Including binary \"generate_msd\"")
file(GLOB_RECURSE GENERATE_MSD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/generate_msd/src/*.cpp)
add_executable(generate_msd ${GENERATE_MSD_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(generate_msd ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ transform_model ------
MESSAGE(STATUS "Including binary \"transform_model\"")
file(GLOB_RECURSE TRANSFORM_MODEL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/transform_model/src/*.cpp)
add_executable(transform_model ${TRANSFORM_MODEL_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(transform_model ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ filter_pointcloud ------
MESSAGE(STATUS "Including binary \"filter_pointcloud\"")
file(GLOB_RECURSE FILTER_POINTCLOUD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/filter_pointcloud/*.cpp)
add_executable(filter_pointcloud ${FILTER_POINTCLOUD_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(filter_pointcloud ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ find_doors ------
MESSAGE(STATUS "Including binary \"find_doors\"")
file(GLOB_RECURSE FIND_DOORS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/find_doors/src/*.cpp)
add_executable(find_doors ${FIND_DOORS_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(find_doors ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ screenshot_pointcloud ------
MESSAGE(STATUS "Including binary \"screenshot_pointcloud\"")
file(GLOB_RECURSE SCREENSHOT_POINTCLOUD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/screenshot_pointcloud/src/*.cpp)
add_executable(screenshot_pointcloud ${SCREENSHOT_POINTCLOUD_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(screenshot_pointcloud ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ split_image_by_floorplan ------
MESSAGE(STATUS "Including binary \"split_image_by_floorplan\"")
file(GLOB_RECURSE SPLIT_IMG_BY_FP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/split_image_by_floorplan/src/*.cpp)
add_executable(split_image_by_floorplan ${SPLIT_IMG_BY_FP_SRC} $<TARGET_OBJECTS:core>)
target_link_libraries(split_image_by_floorplan ${XERCESC_LIBRARY} ${OpenCV_LIBS} ${Boost_LIBRARIES})

# ------ surface_carve ------
MESSAGE(STATUS "Including binary \"surface_carve\"")
file(GLOB_RECURSE SURFACE_CARVE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/execs/surface_carve/src/*.cpp)
add_executable(surface_carve ${SURFACE_CARVE_SRC})

