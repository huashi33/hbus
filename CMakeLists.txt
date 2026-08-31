cmake_minimum_required(VERSION 3.10)
project(hbus CXX C)

set(CMAKE_BUILD_TYPE Debug)
set(3RD_DIR /home/huashi/code)
include_directories(
  ${3RD_DIR}/include
  ${CMAKE_SOURCE_DIR}/include
)
link_directories(
  ${3RD_DIR}/lib
)

# set(CMAKE_BUILD_TYPE Release)
# file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/*.cc)
add_library(hbus SHARED src/hcommon.cc
                        src/hnode.cc
                        )
target_link_libraries(hbus PRIVATE nng)


# file(GLOB SERVER_FILES ${CMAKE_SOURCE_DIR}/broker/*.cc)
add_executable(hbroker src/hbroker.cc)
target_link_libraries(hbroker PRIVATE nng)

add_subdirectory(test)