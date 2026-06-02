# =============================================================================
# C Language Dialect Configuration & Validation (C11 and Above)
# =============================================================================

# Define the user-facing option defaulting to C17 ("17")
set(C_DIALECT "17" CACHE STRING "Specifies the C language standard version (11, 17, 23, 26)")

# Provide a drop-down hint list for IDE layouts and CMake-GUI instances
set_property(CACHE C_DIALECT PROPERTY STRINGS "11" "17" "23" "26")

# Normalize and strip any trailing/leading whitespace from user input
string(STRIP "${C_DIALECT}" C_DIALECT_SANITIZED)

# Define the explicit whitelist of supported modern C standards
set(ALLOWED_C_DIALECTS "11" "17" "23" "26")

# Validate the input against our modern whitelisted standards
if(NOT "${C_DIALECT_SANITIZED}" IN_LIST ALLOWED_C_DIALECTS)
    message(WARNING
        " Unsupported or invalid value '${C_DIALECT}' assigned to C_DIALECT.\n"
        " This project strictly targets modern C environments and requires C11 or newer.\n"
        " Valid numeric options are: 11, 17, 23, 26.\n"
        " NOTE: Do not include compiler prefixes (e.g., write '11' instead of 'gnu11').\n"
        " Resetting configuration to default standard: C17."
    )
    # Reset the cache variable forcefully to ensure consistency in the GUI/CLI state
    set(C_DIALECT "17" CACHE STRING "Specifies the C language standard version (11, 17, 23, 26)" FORCE)
    set(C_DIALECT_SANITIZED "17")
endif()

# Map the validated choice directly to CMake's internal standard target variables
set(CMAKE_C_STANDARD "${C_DIALECT_SANITIZED}")
set(CMAKE_C_STANDARD_REQUIRED ON)

# Control compiler-specific extensions orthogonally if desired (Default: ON to match legacy POSIX needs)
# set(CMAKE_C_EXTENSIONS ON CACHE BOOL "Enable compiler-specific extensions")
set(CMAKE_C_EXTENSIONS OFF)

