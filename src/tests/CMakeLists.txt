cmake_minimum_required(VERSION 2.8)
# tests CMAKE

INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/inference)

SET(TESTS_H
    factor_features_level_test.h)

SET(TESTS_CPP
    unit_tests.cpp)

ADD_LIBRARY(tests ${TESTS_H} ${TESTS_CPP})

if (test)
  enable_testing()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})
  INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/tests)

  ADD_EXECUTABLE(unit_tests unit_tests.cpp)
  TARGET_LINK_LIBRARIES(unit_tests inference pthread gtest)
  add_test(unit_tests unit_tests)
endif()
