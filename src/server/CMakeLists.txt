cmake_minimum_required(VERSION 2.8)

INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/inference)

SET(NICE2SERVER_H
    nice2server.h server_log.h)
SET(NICE2SERVER_CPP
    nice2server.cpp server_log.cpp main.cpp)

ADD_EXECUTABLE(nice2server ${NICE2SERVER_H} ${NICE2SERVER_CPP})
TARGET_LINK_LIBRARIES(nice2server base inference gflags glog jsoncpp jsonrpccpp-common jsonrpccpp-server)
