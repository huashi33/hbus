cmake_minimum_required(VERSION 3.10)
project(hbus C)

set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(3RD_DIR /home/huashi/code)
include_directories(
  ${3RD_DIR}/include
  ${CMAKE_SOURCE_DIR}/include
)
link_directories(
  ${3RD_DIR}/lib
)

# set(CMAKE_BUILD_TYPE Release)
# file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/*.c)
add_library(hbus SHARED
    src/hcommon.c
    src/hnode.c
)

target_link_libraries(hbus PRIVATE
    ${3RD_DIR}/lib/libhcore.a
    ${3RD_DIR}/lib/libnng.a
)


# file(GLOB SERVER_FILES ${CMAKE_SOURCE_DIR}/broker/*.cc)
add_executable(hbroker
    src/hbroker.c
)

target_link_libraries(hbroker PRIVATE
    ${3RD_DIR}/lib/libhcore.a
    ${3RD_DIR}/lib/libnng.a
)

# add_subdirectory(test)