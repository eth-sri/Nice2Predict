cmake_minimum_required(VERSION 2.8)

find_package(Protobuf REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})

INCLUDE_DIRECTORIES(${NICE2_SOURCE_DIR}/base ${PROTO_SRC_DIR})

SET(INFERENCE_H
    graph_inference.h
    inference.h
    label_checker.h)
    
SET(INFERENCE_CPP
    graph_inference.cpp
    inference.cpp
    label_checker.cpp
    ${PROTO_SRC_DIR}/service.pb.cc)

ADD_LIBRARY(inference ${INFERENCE_H} ${INFERENCE_CPP})
TARGET_LINK_LIBRARIES(inference base gflags glog jsoncpp
    protobuf::libprotobuf)
