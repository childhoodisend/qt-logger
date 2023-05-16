cmake_minimum_required(VERSION 3.5)

project(qt-logger)

find_package(Qt${QTVERSION} COMPONENTS Core REQUIRED)

add_library(qt-logger STATIC
        loggertypes.h
        logger.cpp
        logger.h
        )

target_link_libraries(qt-logger PRIVATE Qt${QTVERSION}::Core)
