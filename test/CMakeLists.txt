cmake_minimum_required(VERSION 3.10)

# Test executable
set(TEST_SOURCES
    test_main.c
    test_helpers.c
)

add_executable(c7zip-tests ${TEST_SOURCES})

target_include_directories(c7zip-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(c7zip-tests c7zip)

# Set RPATH so the test executable can find libc7zip.so at runtime
if(UNIX AND NOT APPLE)
    set_target_properties(c7zip-tests PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
elseif(APPLE)
    set_target_properties(c7zip-tests PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
endif()

# Copy test archives to build directory
file(GLOB TEST_ARCHIVES "${CMAKE_CURRENT_SOURCE_DIR}/archives/*")
foreach(ARCHIVE ${TEST_ARCHIVES})
    get_filename_component(ARCHIVE_NAME ${ARCHIVE} NAME)
    configure_file(${ARCHIVE} ${CMAKE_CURRENT_BINARY_DIR}/archives/${ARCHIVE_NAME} COPYONLY)
endforeach()

# Create archives directory in build dir if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/archives)

# Register with CTest
enable_testing()
add_test(
    NAME c7zip-tests
    COMMAND c7zip-tests ${CMAKE_CURRENT_BINARY_DIR}/archives
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# On Windows, add the directory with c7zip.dll to PATH for the test
if(WIN32)
    set_tests_properties(c7zip-tests PROPERTIES
        ENVIRONMENT "PATH=$<TARGET_FILE_DIR:c7zip>;$ENV{PATH}"
    )
endif()
