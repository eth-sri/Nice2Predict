cmake_minimum_required(VERSION 2.8)

PROJECT(NICE2)

ADD_DEFINITIONS("-Wall -pedantic")
SET(CMAKE_C_FLAGS "-Wno-long-long -std=c99 -g")
SET(CMAKE_CXX_FLAGS "-Wno-long-long -std=c++11 -g")
#SET(CMAKE_EXE_LINKER_FLAGS "-flto")

ADD_SUBDIRECTORY(base)
ADD_SUBDIRECTORY(inference)
ADD_SUBDIRECTORY(training)
ADD_SUBDIRECTORY(server)
ADD_SUBDIRECTORY(tests)

option(test "Build all tests." OFF)

if (test)
  enable_testing()
endif()
