cmake_minimum_required(VERSION 3.4.1)
project(respeakerws VERSION 1.0.0)

set(MIN_LIBRESPEAKER_VERSION 2.1.1)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-fPIC -std=c++11 -fpermissive")

include(FindPkgConfig)

pkg_check_modules(RESPEAKER REQUIRED respeaker>=${MIN_LIBRESPEAKER_VERSION})

add_executable(respeakerws src/respeakerws.cpp)

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/libs
    ${RESPEAKER_INCLUDE_DIRS}
)

link_directories(
    ${RESPEAKER_LIBRARY_DIRS}
)

find_library(IXWEBSOCKET ixwebsocket lib)

target_link_libraries(respeakerws
    ${LIBGFLAGS_PATH} -lpthread -lz
    ${RESPEAKER_LIBRARIES}
    ${IXWEBSOCKET}
)
