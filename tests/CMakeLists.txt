set(TEST_EXE ${PROJECT_NAME}_test)

add_executable(${TEST_EXE}
        ${CMAKE_SOURCE_DIR}/tests/vtc_tests.cpp
        ${CMAKE_SOURCE_DIR}/thirdparty/pugixml/pugixml.cpp)

set(INCLUDE_DIR
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/thirdparty")

target_include_directories(
        ${TEST_EXE}
        PRIVATE ${INCLUDE_DIR})

configure_file(${CMAKE_SOURCE_DIR}/tests/hilsci.xml ${PROJECT_BINARY_DIR}/tests COPYONLY)

if (WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/dev/vtcdev.dll ${PROJECT_BINARY_DIR}/tests COPYONLY)
else ()
    configure_file(${CMAKE_SOURCE_DIR}/dev/vtcdev.so ${PROJECT_BINARY_DIR}/tests/ COPYONLY)
endif ()