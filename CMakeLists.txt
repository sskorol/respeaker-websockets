cmake_minimum_required(VERSION 3.4.1)
project(respeaker_core VERSION 1.0.0)

set(MIN_LIBRESPEAKER_VERSION 2.1.1)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-fPIC -std=c++14 -fpermissive -fomit-frame-pointer")

include(FindPkgConfig)

pkg_check_modules(RESPEAKER REQUIRED respeaker>=${MIN_LIBRESPEAKER_VERSION})

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/libs
    ${RESPEAKER_INCLUDE_DIRS}
)

file(GLOB PIXEL_RING_SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")
add_executable(respeaker_core src/respeaker_core.cpp ${PIXEL_RING_SOURCES} ${PROJECT_SOURCE_DIR}/src/ws_transport.cpp)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

link_directories(
    ${RESPEAKER_LIBRARY_DIRS}
)

find_library(IXWEBSOCKET ixwebsocket lib)

target_link_libraries(
    respeaker_core
    ${LIBGFLAGS_PATH} -lpthread -lz
    ${RESPEAKER_LIBRARIES}
    ${IXWEBSOCKET}
)
