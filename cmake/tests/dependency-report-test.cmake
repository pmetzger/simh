include("${CMAKE_CURRENT_LIST_DIR}/../dependency-report.cmake")

function(assert_equal actual expected label)
    if (NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "${label}: expected '${expected}', got '${actual}'")
    endif ()
endfunction()

function(assert_contains haystack needle label)
    string(FIND "${haystack}" "${needle}" _position)
    if (_position EQUAL -1)
        message(FATAL_ERROR
            "${label}: expected output to contain '${needle}'")
    endif ()
endfunction()

function(clear_missing_dependencies)
    set_property(GLOBAL PROPERTY ZIMH_MISSING_DEPENDENCY_IDS)
endfunction()

set(ZIMH_DEPENDENCY_HINT_PLATFORM NetBSD)
zimh_dependency_package_hint(hint LIBSLIRP)
assert_equal("${hint}" "NetBSD package: pkgin install libslirp"
             "NetBSD libslirp hint")

set(ZIMH_DEPENDENCY_HINT_PLATFORM Debian)
zimh_dependency_package_hint(hint SDL2_TTF)
assert_equal("${hint}" "Debian/Ubuntu package: apt install libsdl2-ttf-dev"
             "Debian SDL2_ttf hint")

set(ZIMH_DEPENDENCY_HINT_PLATFORM Fedora)
zimh_dependency_package_hint(hint CMOCKA)
assert_equal("${hint}" "Fedora/RHEL package: dnf install libcmocka-devel"
             "Fedora cmocka hint")

zimh_dependency_package_hint(hint PCAP)
assert_equal("${hint}" "Fedora/RHEL package: dnf install libpcap-devel"
             "Fedora libpcap hint")

set(ZIMH_DEPENDENCY_HINT_PLATFORM Arch)
zimh_dependency_package_hint(hint VDE)
assert_equal("${hint}" "Arch package: pacman -S vde2" "Arch VDE hint")

set(ZIMH_DEPENDENCY_HINT_PLATFORM MacPorts)
zimh_dependency_package_hint(hint SDL2)
assert_equal("${hint}" "MacPorts package: sudo port install libsdl2"
             "MacPorts SDL2 hint")

set(ZIMH_DEPENDENCY_HINT_PLATFORM Homebrew)
zimh_dependency_package_hint(hint SDL2)
assert_equal("${hint}" "Homebrew package: brew install sdl2"
             "Homebrew SDL2 hint")

set(ZIMH_DEPENDENCY_HINT_PLATFORM MSYS2)
set(ZIMH_DEPENDENCY_HINT_MSYS2_PREFIX mingw-w64-ucrt-x86_64)
zimh_dependency_package_hint(hint PNG)
assert_equal("${hint}"
             "MSYS2 package: pacman -S mingw-w64-ucrt-x86_64-libpng"
             "MSYS2 libpng hint")

set(ZIMH_DEPENDENCY_HINT_PLATFORM MSVC)
zimh_dependency_package_hint(hint SDL2_TTF)
assert_equal("${hint}" "MSVC/vcpkg package: vcpkg install sdl2-ttf"
             "MSVC SDL2_ttf hint")

zimh_dependency_package_hint(hint VDE)
assert_equal("${hint}" "MSVC note: VDE is not available for Windows builds"
             "MSVC VDE hint")

clear_missing_dependencies()
zimh_has_missing_dependencies(has_missing)
assert_equal("${has_missing}" "FALSE" "empty missing-dependency state")

zimh_record_missing_dependency(
    NAME "libslirp"
    VERSION ">= 4.7.0"
    REASON "WITH_NETWORK=ON and WITH_SLIRP=ON"
    PROBE "pkg-config package slirp >= 4.7.0"
    DISABLE "-DWITH_SLIRP=OFF"
    PACKAGE_KEY LIBSLIRP)
zimh_has_missing_dependencies(has_missing)
assert_equal("${has_missing}" "TRUE" "recorded missing-dependency state")

zimh_format_missing_dependencies(report)
assert_contains("${report}"
                "ZIMH could not be configured because required dependencies"
                "report header")
assert_contains("${report}" "  libslirp >= 4.7.0" "report title")
assert_contains("${report}"
                "Needed because WITH_NETWORK=ON and WITH_SLIRP=ON."
                "report reason")
assert_contains("${report}" "MSVC/vcpkg package: vcpkg install libslirp"
                "report package hint")
assert_contains("${report}" "cmake -DWITH_SLIRP=OFF ..." "report disable hint")

message(STATUS "dependency-report-test passed")
