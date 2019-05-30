include_directories(${CMAKE_SOURCE_DIR}/src)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)

add_executable(trace_pread64 trace_pread64.cc)
target_link_libraries(trace_pread64 dw_bcc-static)
target_link_libraries(trace_pread64 elf)
target_link_libraries(trace_pread64 unwind-x86_64)
target_link_libraries(trace_pread64 unwind)
target_link_libraries(trace_pread64 pthread)
target_link_libraries(trace_pread64 bcc)
