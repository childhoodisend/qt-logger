cmake_minimum_required(VERSION 3.5)

find_package(Qt${QTVERSION} COMPONENTS Core REQUIRED)

add_library(Logger SHARED
    loggertypes.h
    logger.cpp
    logger.h
  )

target_link_libraries(Logger PRIVATE Qt${QTVERSION}::Core)

#target_include_directories(Logger PRIVATE ${NDCS_COMMON_FOLDER})


#IF (UNIX)
#    add_custom_command(TARGET Logger POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Logger> ${NDCS_BUILD_DIR}/$<TARGET_FILE_NAME:Logger>
#        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:Logger> ${NDCS_LIBS_DIR}/$<TARGET_LINKER_FILE_NAME:Logger>
#    )
#ENDIF()
