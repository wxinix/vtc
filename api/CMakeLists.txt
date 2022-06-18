set(VTCAPI_LIB vtcapi)

if (WIN32)
    set(VTCAPI_SRC
            ${CMAKE_SOURCE_DIR}/api/vtcapi.cpp
            ${CMAKE_SOURCE_DIR}/api/vtcapi.rc)
else ()
    set(VTCAPI_SRC
            ${CMAKE_SOURCE_DIR}/api/vtcapi.cpp)
endif ()

add_library(${VTCAPI_LIB} SHARED ${VTCAPI_SRC})

set(INCLUDE_DIR
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/thirdparty")

target_include_directories(
        ${VTCAPI_LIB}
        PRIVATE ${INCLUDE_DIR})
