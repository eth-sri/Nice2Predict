cmake_minimum_required(VERSION 2.8)
# base CMAKE

SET(BASE_H
    base.h
    stringprintf.h
    stringset.h
    strutil.h)
SET(BASE_CPP
    base.cpp
    stringprintf.cpp
    stringset.cpp
    strutil.cpp)

ADD_LIBRARY(base ${BASE_H} ${BASE_CPP})
