include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

configure_file(libdw_bcc.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libdw_bcc.pc @ONLY)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wno-unused-result")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdeclaration-after-statement")

file(GLOB libdw_bcc_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
add_library(dw_bcc-static STATIC ${libdw_bcc_sources})
set_target_properties(dw_bcc-static PROPERTIES OUTPUT_NAME dw_bcc)
add_library(dw_bcc-shared SHARED ${libdw_bcc_sources})
set_target_properties(dw_bcc-shared PROPERTIES VERSION ${REVISION_LAST} SOVERSION 0)
set_target_properties(dw_bcc-shared PROPERTIES OUTPUT_NAME dw_bcc)

set(dw_bcc_api_headers libdw_bcc.h ptrace.h types.h)

install(TARGETS dw_bcc-shared LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${dw_bcc_api_headers} DESTINATION include/bcc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libdw_bcc.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
