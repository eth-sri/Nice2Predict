cmake_minimum_required(VERSION 2.8)

INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/base)

SET(INFERENCE_H
    graph_inference.h
    inference.h
    label_checker.h)
SET(INFERENCE_CPP
    graph_inference.cpp
    inference.cpp
    label_checker.cpp)

ADD_LIBRARY(inference ${INFERENCE_H} ${INFERENCE_CPP})
TARGET_LINK_LIBRARIES(inference base gflags glog jsoncpp)

if (test)
  enable_testing()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})

  ADD_EXECUTABLE(inference_test inference_test.cpp)
  TARGET_LINK_LIBRARIES(inference_test inference pthread gtest)
  add_test(inference_test inference_test)
endif()

