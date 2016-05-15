cmake_minimum_required(VERSION 2.8)
# tests CMAKE

INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/inference)

SET(TESTS_H)

SET(TESTS_CPP
    inference_test.cpp
    factor_features_level_test.cpp)

ADD_LIBRARY(tests ${TESTS_H} ${TESTS_CPP})

if (test)
  enable_testing()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})
  INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/tests)

  ADD_EXECUTABLE(inference_test inference_test.cpp)
  TARGET_LINK_LIBRARIES(inference_test inference pthread gtest)
  add_test(inference_test inference_test)
endif()
