cmake_minimum_required(VERSION 3.4.1)
project(RespeakerCore VERSION 1.0.0)

set(MIN_LIBRESPEAKER_VERSION 2.1.1)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-fPIC -std=c++14 -fpermissive")

include(FindPkgConfig)

pkg_check_modules(RESPEAKER REQUIRED respeaker>=${MIN_LIBRESPEAKER_VERSION})

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/libs
    ${RESPEAKER_INCLUDE_DIRS}
)

add_executable(RespeakerCore src/RespeakerCore.cpp)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/config.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

link_directories(
    ${RESPEAKER_LIBRARY_DIRS}
)

find_library(IXWEBSOCKET ixwebsocket lib)
find_library(PAHO_MQTT_3A paho-mqtt3a lib)

target_link_libraries(RespeakerCore
    ${LIBGFLAGS_PATH} -lpthread -lz
    ${RESPEAKER_LIBRARIES}
    ${IXWEBSOCKET}
    ${PAHO_MQTT_3A}
)
