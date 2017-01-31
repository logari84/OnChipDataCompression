project(OnChipDataCompression)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/modules")

find_package(ROOT REQUIRED COMPONENTS Core Hist RIO Tree Physics Graf Gpad Matrix)
find_package(BoostEx REQUIRED COMPONENTS program_options filesystem regex system)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS} ${ROOT_INCLUDE_DIR})

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++14 -Wall -O3")

find_file(scram_path "scram")
if(scram_path)
    set(CMSSW_RELEASE_BASE_SRC "$ENV{CMSSW_RELEASE_BASE}/src")
else()
    set(CMSSW_RELEASE_BASE_SRC "$ENV{CMSSW_RELEASE_BASE_SRC}")
endif()

include_directories(SYSTEM "${CMSSW_RELEASE_BASE_SRC}")

get_filename_component(CMSSW_BASE_SRC "${PROJECT_SOURCE_DIR}" DIRECTORY)
include_directories("${CMSSW_BASE_SRC}")

file(GLOB_RECURSE HEADER_LIST "*.h")
add_custom_target(headers SOURCES ${HEADER_LIST})

file(GLOB_RECURSE ALGO_SOURCE_LIST "Algorithms/src/*.cc")
add_library("OnChipDataCompressionAlgorithms" OBJECT ${ALGO_SOURCE_LIST})

file(GLOB_RECURSE OTHER_SOURCE_LIST "*.cc")
add_custom_target(other_sources SOURCES ${OTHER_SOURCE_LIST})

file(GLOB_RECURSE SCRIPT_LIST "*.py")
add_custom_target(scripts SOURCES ${SCRIPT_LIST})

file(GLOB_RECURSE CONFIG_LIST "*.xml")
add_custom_target(configs SOURCES ${CONFIG_LIST})


