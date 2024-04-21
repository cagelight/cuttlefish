cmake_minimum_required(VERSION 3.17.0)

set(ProjectName "cuttlefish" CACHE STRING "Project Name")
project(${ProjectName})

set(CMAKE_CXX_STANDARD 17)

set( CMAKE_CONFIGURATION_TYPES "Debug;Release;Native" )
set( CMAKE_CXX_FLAGS         "" )
set( CMAKE_CXX_FLAGS_DEBUG   "-Wall -Wextra -Og -march=core2 -mtune=native -ggdb3" )
set( CMAKE_CXX_FLAGS_RELEASE "-w -O2 -march=core2 -mtune=generic" )
set( CMAKE_CXX_FLAGS_NATIVE  "-w -Os --fast-math -march=native -mtune=native" )

if( NOT CMAKE_BUILD_TYPE )
	set( CMAKE_BUILD_TYPE Release )
endif()

set(ProjectDir "${CMAKE_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RELEASE")
endif()

set(ProjectIncludeDirectories ${CMAKE_CURRENT_BINARY_DIR})
set(ProjectLibs "-lpthread")

file(GLOB ProjectFiles
    "${ProjectDir}/src/*.hh"
    "${ProjectDir}/src/*.cc"
)
source_group("project" FILES ${ProjectFiles})

find_package(Qt6 COMPONENTS Widgets Core5Compat REQUIRED)
find_package(OpenCV COMPONENTS core imgproc features2d REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(ProjectBinary "cuttlefish")
set(ProjectOldBinary "cuttleold")

add_executable(${ProjectBinary} ${ProjectFiles})
install(TARGETS ${ProjectBinary} RUNTIME DESTINATION "bin/")
set_target_properties(${ProjectBinary} PROPERTIES INCLUDE_DIRECTORIES ${ProjectIncludeDirectories})
set_target_properties(${ProjectBinary} PROPERTIES PROJECT_LABEL "${ProjectName}")
target_link_libraries(${ProjectBinary} ${ProjectLibs} Qt6::Widgets Qt6::Core5Compat opencv_core opencv_imgproc opencv_features2d)
