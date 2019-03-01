cmake_minimum_required(VERSION 2.8.11)

set(ProjectName "cuttlefish" CACHE STRING "Project Name")
project(${ProjectName})
set(CMAKE_BUILD_TYPE "DEBUG")

set(ProjectDir "${CMAKE_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RELEASE")
endif()

set(ProjectIncludeDirectories ${CMAKE_CURRENT_BINARY_DIR})
set(ProjectLibs "-lpthread -lasterales")

file(GLOB ProjectFiles
    "${ProjectDir}/src/*.hh"
    "${ProjectDir}/src/*.cc"
)
source_group("project" FILES ${ProjectFiles})

file(GLOB ProjectOldFiles
    "${ProjectDir}/src/old/*.hh"
    "${ProjectDir}/src/old/*.cc"
	"${ProjectDir}/src/imgview.cc"
)
source_group("project" FILES ${ProjectOldFiles})

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(ProjectBinary "cuttlefish")
set(ProjectOldBinary "cuttleold")

add_executable(${ProjectBinary} ${ProjectFiles})
set(CMAKE_INSTALL_PREFIX "/usr/local/")
install(TARGETS ${ProjectBinary} RUNTIME DESTINATION "bin/")
set_target_properties(${ProjectBinary} PROPERTIES INCLUDE_DIRECTORIES ${ProjectIncludeDirectories})
set_target_properties(${ProjectBinary} PROPERTIES PROJECT_LABEL "${ProjectName}")
target_link_libraries(${ProjectBinary} ${ProjectLibs} Qt5::Widgets)

add_executable(${ProjectOldBinary} ${ProjectOldFiles})
set(CMAKE_INSTALL_PREFIX "/usr/local/")
install(TARGETS ${ProjectOldBinary} RUNTIME DESTINATION "bin/")
set_target_properties(${ProjectOldBinary} PROPERTIES INCLUDE_DIRECTORIES ${ProjectIncludeDirectories})
set_target_properties(${ProjectOldBinary} PROPERTIES PROJECT_LABEL "${ProjectName}")
target_link_libraries(${ProjectOldBinary} ${ProjectLibs} Qt5::Widgets)
