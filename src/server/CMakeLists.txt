cmake_minimum_required(VERSION 2.8)

find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)

grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})

INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/base)
INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/inference)
INCLUDE_DIRECTORIES(${PROTO_SRC_DIR})

add_executable(nice2serverproto
    nice2serverproto.cpp
    server_log.cpp
    ${PROTO_SRC_DIR}/service.pb.cc
    ${GRPC_SRCS})

target_link_libraries(nice2serverproto base inference gflags glog
    gRPC::grpc++_reflection
    protobuf::libprotobuf)

SET(NICE2SERVER_H
    nice2server.h server_log.h)
SET(NICE2SERVER_CPP
    nice2server.cpp server_log.cpp main.cpp
    ${PROTO_SRC_DIR}/service.pb.cc)

ADD_EXECUTABLE(nice2server ${NICE2SERVER_H} ${NICE2SERVER_CPP})
TARGET_LINK_LIBRARIES(nice2server base inference gflags glog jsoncpp jsonrpccpp-common jsonrpccpp-server
    gRPC::grpc++_reflection
    protobuf::libprotobuf)
