cmake_minimum_required(VERSION 2.8)

project(libcz7ip)

set(INCLUDE_DIR include)

set(lib7zip_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib7zip/include)
set(lib7zip_LIB ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib7zip/lib/lib7zip.a)

include_directories(
  ${INCLUDE_DIR}
  ${lib7zip_INCLUDE_DIR}
)

set (SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set (SOURCES
    ${SOURCE_DIR}/context.cc
    ${SOURCE_DIR}/utf_convert.cc
)

add_library(c7zip SHARED ${SOURCES})

target_compile_options(c7zip PRIVATE -Wall -Wno-missing-braces -std=c++11)

target_link_libraries(c7zip ${lib7zip_LIB})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(c7zip -ldl)
endif() # Linux

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_link_libraries(c7zip -lole32 -loleaut32 -luuid)
endif() # Windows

set (SAMPLE_SOURCES
    ${SOURCE_DIR}/sample.c
)

add_executable(c7zip-sample ${SAMPLE_SOURCES})
target_link_libraries(c7zip-sample c7zip)
