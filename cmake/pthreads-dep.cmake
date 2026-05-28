#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
# Manage the pthreads dependency
#
# (a) Try to locate the system's installed pthreads library, which is very
#     platform dependent (MSVC -> Pthreads4w, MinGW -> pthreads, *nix -> pthreads.)
#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

add_library(thread_lib INTERFACE)
set(AIO_FLAGS)

if (WITH_ASYNC)
    if (MSVC OR (WIN32 AND CMAKE_C_COMPILER_ID MATCHES ".*Clang.*with MSVC-like command-line.*"))
        # Pthreads4w: pthreads for windows.
        if (USING_VCPKG)
            find_package(PThreads4W REQUIRED)
            target_link_libraries(thread_lib INTERFACE PThreads4W::PThreads4W)
            set(THREADING_PKG_STATUS "vcpkg PThreads4W")
        else ()
            find_package(PTW)

            if (PTW_FOUND)
                target_compile_definitions(thread_lib INTERFACE PTW32_STATIC_LIB)
                target_include_directories(thread_lib INTERFACE ${PTW_INCLUDE_DIRS})
                target_link_libraries(thread_lib INTERFACE ${PTW_C_LIBRARY})

                set(THREADING_PKG_STATUS "detected PTW/PThreads4W")
            else ()
                set(THREADING_PKG_STATUS
                    "PThreads4W not found; asynchronous I/O disabled")
            endif ()
        endif ()
    else ()
        # Let CMake determine which threading library ought be used.
        set(THREADS_PREFER_PTHREAD_FLAG On)
        find_package(Threads)
        if (THREADS_FOUND)
          target_link_libraries(thread_lib INTERFACE Threads::Threads)
        endif (THREADS_FOUND)

        set(THREADING_PKG_STATUS "Platform-detected threading support")
    endif ()

    if (THREADS_FOUND OR PTW_FOUND OR PThreads4W_FOUND)
        set(AIO_FLAGS USE_READER_THREAD SIM_ASYNCH_IO)
    else ()
        set(AIO_FLAGS DONT_USE_READER_THREAD)
    endif ()
else()
    target_compile_definitions(thread_lib INTERFACE DONT_USE_READER_THREAD)
    set(THREADING_PKG_STATUS "asynchronous I/O disabled.")
endif()
