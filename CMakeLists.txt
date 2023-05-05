cmake_minimum_required(VERSION 3.10)
project(respeaker_core VERSION 1.0.0)

set(MIN_LIBRESPEAKER_VERSION 2.1.1)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS "-fPIC -fomit-frame-pointer -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++14 -fpermissive")

include(FindPkgConfig)

pkg_check_modules(RESPEAKER REQUIRED respeaker>=${MIN_LIBRESPEAKER_VERSION})

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${RESPEAKER_INCLUDE_DIRS}
)

file(GLOB PIXEL_RING_SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")
add_executable(respeaker_core src/main.cpp src/log.cpp ${PIXEL_RING_SOURCES} ${PROJECT_SOURCE_DIR}/src/ws_transport.cpp ${PROJECT_SOURCE_DIR}/src/respeaker_core.cpp ${PROJECT_SOURCE_DIR}/src/config.cpp)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/models DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

link_directories(
    ${RESPEAKER_LIBRARY_DIRS}
)

find_package(ixwebsocket REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

target_link_libraries(
    respeaker_core
    ${LIBGFLAGS_PATH} -lpthread -lz -lstdc++fs -lasound
    ${RESPEAKER_LIBRARIES}
    ixwebsocket::ixwebsocket
    spdlog::spdlog
    fmt::fmt
)
