cmake_minimum_required(VERSION 3.10)
project(hbus CXX C)


set(3RD_DIR /home/huashi/code)
include_directories(
  ${3RD_DIR}/include
  ${CMAKE_SOURCE_DIR}/include
)
link_directories(
  ${3RD_DIR}/lib
)

set(CMAKE_BUILD_TYPE Release)
file(GLOB COMMON_FILES ${CMAKE_SOURCE_DIR}/common/*.cc)
file(GLOB CLIENT_FILES ${CMAKE_SOURCE_DIR}/client/*.cc)
add_library(hbus SHARED ${CLIENT_FILES})
target_link_libraries(hbus PRIVATE nng)


file(GLOB SERVER_FILES ${CMAKE_SOURCE_DIR}/broker/*.cc)
add_executable(hbusbroker ${SERVER_FILES} ${COMMON_FILES})
target_link_libraries(hbusbroker PRIVATE nng)

add_subdirectory(test)