include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CMakePushCheckState)

function(configure_uuid_feature target)
    if (WIN32)
        target_link_libraries(${target} INTERFACE rpcrt4)
        return()
    endif ()

    check_include_file(uuid/uuid.h HAVE_UUID_UUID_H)
    if (HAVE_UUID_UUID_H)
        cmake_push_check_state()
        check_symbol_exists(uuid_generate uuid/uuid.h HAVE_UUID_GENERATE)
        cmake_pop_check_state()

        if (NOT HAVE_UUID_GENERATE)
            find_library(UUID_LIBRARY uuid)
            if (UUID_LIBRARY)
                cmake_push_check_state()
                list(APPEND CMAKE_REQUIRED_LIBRARIES "${UUID_LIBRARY}")
                check_symbol_exists(uuid_generate uuid/uuid.h
                                    HAVE_UUID_GENERATE_WITH_LIBUUID)
                cmake_pop_check_state()
            endif ()
        endif ()
    endif ()

    if (HAVE_UUID_GENERATE)
        target_compile_definitions(${target} INTERFACE HAVE_UUID_GENERATE)
        return()
    endif ()

    if (HAVE_UUID_GENERATE_WITH_LIBUUID)
        target_compile_definitions(${target} INTERFACE HAVE_UUID_GENERATE)
        target_link_libraries(${target} INTERFACE "${UUID_LIBRARY}")
        return()
    endif ()

    check_include_file(uuid.h HAVE_UUID_H)
    if (HAVE_UUID_H)
        cmake_push_check_state()
        check_c_source_compiles("
            #include <stdint.h>
            #include <uuid.h>

            int main(void)
            {
                unsigned char out[16];
                uuid_t uuid;
                uint32_t status = uuid_s_ok;

                uuid_create(&uuid, &status);
                if (status != uuid_s_ok)
                    return 1;
                uuid_enc_be(out, &uuid);
                return 0;
            }
        " HAVE_UUID_CREATE_API)
        cmake_pop_check_state()
    endif ()

    if (HAVE_UUID_CREATE_API)
        target_compile_definitions(${target} INTERFACE HAVE_UUID_CREATE)
        return()
    endif ()

    if (COMMAND zimh_dependency_package_hint)
        zimh_dependency_package_hint(_uuid_package_hint UUID)
    endif ()

    if (_uuid_package_hint)
        set(_uuid_package_text "\n${_uuid_package_hint}")
    else ()
        set(_uuid_package_text
            "\nInstall the host UUID development package, such as "
            "uuid-dev on Debian/Ubuntu, libuuid-devel on Fedora/RHEL, "
            "or util-linux on Arch Linux.")
    endif ()

    message(FATAL_ERROR
        "uuid_generate(), libuuid, or uuid_create() with uuid_enc_be() "
        "is required for UUID generation on this host.${_uuid_package_text}")
endfunction()
