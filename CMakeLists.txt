cmake_minimum_required(VERSION 3.13)
project(libdw_bcc C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_library(libdw_bcc SHARED
        libdw_bcc.h
        dso.c
        dso.h
        event.c
        event.h
        list.h
        machine.c
        machine.h
        map.c
        map.h
        ptrace.h
        queue.h
        rbtree.c
        rbtree.h
        rbtree_augmented.h
        refcount.h
        rwsem.c
        rwsem.h
        symbol.h
        symbol_elf.c
        thread.c
        thread.h
        types.h
        unwind.h
        unwind_libunwind.c
        utility.c
        utility.h)

set(api_headers libdw_bcc.h machine.h)
install(TARGETS libdw_bcc LIBRARY DESTINATION lib64/)
install(FILES ${api_headers} DESTINATION include/)
