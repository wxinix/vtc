set(EXE_NAME sdlclogger)

add_executable(${EXE_NAME}
        ${CMAKE_SOURCE_DIR}/sdlclogger/sdlclogger.cpp)

set(INCLUDE_DIR
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/thirdparty")

target_include_directories(
        ${EXE_NAME}
        PRIVATE ${INCLUDE_DIR})

if (WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/dev/vtcdev.dll
            ${PROJECT_BINARY_DIR}/sdlclogger/
            COPYONLY)
else ()
    configure_file(${CMAKE_SOURCE_DIR}/dev/vtcdev.so
            ${PROJECT_BINARY_DIR}/sdlclogger/
            COPYONLY)
endif ()