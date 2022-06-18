set(TSMCI_LIB tsmci)

if (WIN32)
    set(TSMCI_SRC
            ${CMAKE_SOURCE_DIR}/tsm/tsmci.cpp
            ${CMAKE_SOURCE_DIR}/tsm/tsmci.rc
            ${CMAKE_SOURCE_DIR}/thirdparty/pugixml/pugixml.cpp)

    configure_file(${CMAKE_SOURCE_DIR}/tsm/sample/tsmci.config.xml ${PROJECT_BINARY_DIR}/tsm COPYONLY)

    add_library(${TSMCI_LIB} SHARED ${TSMCI_SRC})

    set(INCLUDE_DIR
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_SOURCE_DIR}/thirdparty"
            "${PROJECT_BINARY_DIR}/tsm/CMakeFiles/${TSMCI_LIB}.dir")

    target_include_directories(
            ${TSMCI_LIB}
            PRIVATE ${INCLUDE_DIR})
else ()
    message(STATUS "TransModeller Controller Interface tsmci project can only be build for Windows!")
endif ()

