cmake_minimum_required(VERSION 2.8)

set(LAB_BUILD_TESTS OFF CACHE BOOL "Build lab tests")

project(szs)

add_subdirectory(vendor/lab)

set(INCLUDE_DIR include)

set(lib7zip_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib7zip/include)
set(lib7zip_LIB ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib7zip/lib/lib7zip.a)

set(lab_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lab/src)
set(asio_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/asio/include)

add_definitions(-DASIO_STANDALONE -D_WIN32_WINNT=0x0601)

include_directories(
  ${INCLUDE_DIR}
  ${lib7zip_INCLUDE_DIR}
  ${lab_INCLUDE_DIR}
  ${asio_INCLUDE_DIR}
)

set (SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set (SOURCES
    ${SOURCE_DIR}/main.cc
)

add_executable(szs ${SOURCES})

target_compile_options(szs PRIVATE -Wall -Wno-missing-braces -std=c++11)

target_link_libraries(szs ${lib7zip_LIB})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(szs -ldl)
endif() # Linux

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_link_libraries(szs -lole32 -loleaut32 -luuid -lws2_32 -lmswsock)
endif() # Windows