cmake_minimum_required(VERSION 2.8)

INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/inference)

ADD_EXECUTABLE(train train.cpp)
TARGET_LINK_LIBRARIES(train base inference gflags glog jsoncpp)

ADD_EXECUTABLE(eval eval.cpp)
TARGET_LINK_LIBRARIES(eval base inference gflags glog jsoncpp)
