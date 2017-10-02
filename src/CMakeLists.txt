cmake_minimum_required(VERSION 2.8)
PROJECT(NICE2)

set(PROTOS ${NICE2_SOURCE_DIR}/protos/service.proto)

set(PROTO_SRC_DIR ${NICE2_BINARY_DIR}/proto-src)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
include_directories(${PROTO_SRC_DIR})

list(APPEND CMAKE_MODULE_PATH "${NICE2_SOURCE_DIR}/cmake")
include_directories(SYSTEM /usr/local/include)
link_directories(/usr/local/lib)

ADD_DEFINITIONS("-Wall -pedantic")
SET(CMAKE_C_FLAGS "-Wno-long-long -std=c99 -g")
SET(CMAKE_CXX_FLAGS "-Wno-long-long -std=c++11 -g")
#SET(CMAKE_EXE_LINKER_FLAGS "-flto")

ADD_SUBDIRECTORY(base)
ADD_SUBDIRECTORY(inference)
ADD_SUBDIRECTORY(training)
ADD_SUBDIRECTORY(server)
ADD_SUBDIRECTORY(tests)

option(test "Build all tests." ON)

if (test)
  enable_testing()
endif()
