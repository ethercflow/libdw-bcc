include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

configure_file(libdw_bpf.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libdw_bpf.pc @ONLY)

if (CMAKE_VERSION VERSION_LESS "3.1")
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_C_FLAGS "-std=gnu11 ${CMAKE_C_FLAGS}")
  endif ()
else ()
  set(CMAKE_C_STANDARD 11)
endif ()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wno-unused-result")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdeclaration-after-statement")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")

file(GLOB libdw_bpf_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
add_library(dw_bpf-static STATIC ${libdw_bpf_sources})
target_link_libraries(dw_bpf-static LINK_PRIVATE
  ${LIBUNWIND_LIBRARY}
  ${LIBUNWIND_PLATFORM_LIBRARY}
  ${LIBELF_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)
set_target_properties(dw_bpf-static PROPERTIES OUTPUT_NAME dw_bpf)
add_library(dw_bpf-shared SHARED ${libdw_bpf_sources})
target_link_libraries(dw_bpf-shared LINK_PRIVATE
  ${LIBUNWIND_LIBRARIES}
  ${LIBUNWIND_PLATFORM_LIBRARIES}
  ${LIBELF_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)
set_target_properties(dw_bpf-shared PROPERTIES VERSION ${REVISION_LAST} SOVERSION 0)
set_target_properties(dw_bpf-shared PROPERTIES OUTPUT_NAME dw_bpf)

set(dw_bpf_api_headers libdw_bpf.h ptrace.h types.h)

install(TARGETS dw_bpf-shared LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${dw_bpf_api_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dw_bpf)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libdw_bpf.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
