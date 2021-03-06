include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_VERSION VERSION_LESS "3.1")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
  endif ()
else ()
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_STANDARD 11)
endif ()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")

add_executable(syscall syscall.cc)
target_link_libraries(syscall dw_bpf-static)
target_link_libraries(syscall bcc)

add_executable(syscall_parallel syscall_parallel.cc)
target_link_libraries(syscall_parallel dw_bpf-static)
target_link_libraries(syscall_parallel bcc)

add_executable(uprobe uprobe.cc)
target_link_libraries(uprobe dw_bpf-static)
target_link_libraries(uprobe bcc)

add_executable(pwrite64_event pwrite64_event.cc)
target_link_libraries(pwrite64_event dw_bpf-static)
target_link_libraries(pwrite64_event bcc)
