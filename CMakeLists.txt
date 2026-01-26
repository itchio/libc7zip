cmake_minimum_required(VERSION 3.10)

project(libcz7ip)

if (MSVC)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

set(INCLUDE_DIR include)

set(lib7zip_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib7zip/Lib7Zip)
add_subdirectory(vendor/lib7zip)

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

target_compile_options(c7zip PRIVATE -std=c++11)

add_dependencies(c7zip 7zip)
target_link_libraries(c7zip 7zip)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(c7zip -ldl)
endif() # Linux

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_link_libraries(c7zip ole32 oleaut32 uuid)
endif() # Windows

set (SAMPLE_SOURCES
    ${SOURCE_DIR}/sample.c
)

add_executable(c7zip-sample ${SAMPLE_SOURCES})
target_link_libraries(c7zip-sample c7zip)
