cmake_minimum_required(VERSION 3.10)
project(hbus C)

set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 设置第三方库路径
set(3RD_DIR_ROOT "${CMAKE_SOURCE_DIR}/3rd")
set(3RD_LIBDIR "${3RD_DIR_ROOT}/lib")
set(3RD_LIS )

message(STATUS "当前系统名称: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(3RD_LIBDIR "${3RD_LIBDIR}/win64")
    else()
        set(3RD_LIBDIR "${3RD_LIBDIR}/win32")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(3RD_LIBDIR "${3RD_LIBDIR}/linux_aarch64")
    else()
        set(3RD_LIBDIR "${3RD_LIBDIR}/linux_x64")
    endif()
endif()
message(STATUS "3RD_LIBDIR: ${3RD_LIBDIR}")
include_directories(
  ${3RD_DIR_ROOT}/include
  ${CMAKE_SOURCE_DIR}/include
)
link_directories(
  ${3RD_LIBDIR}
)

add_library(hbus SHARED src/hnode.c)
target_link_libraries(hbus PRIVATE libhcore.a libhv_static.a ws2_32 iphlpapi crypt32 secur32)


# file(GLOB SERVER_FILES ${CMAKE_SOURCE_DIR}/broker/*.cc)
add_executable(hbroker src/hbroker.c )
target_link_libraries(hbroker PRIVATE libhcore.a libhv_static.a ws2_32 iphlpapi crypt32 secur32)

add_subdirectory(test) 