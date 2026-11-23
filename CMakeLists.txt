cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_CXX_COMPILER)
    find_program(CMAKE_CXX_COMPILER NAMES clang++-22 clang++)
    if(NOT CMAKE_CXX_COMPILER)
        message(FATAL_ERROR "Neither clang++-22 nor clang++ was found")
    endif()
endif()

# Match the Makefile defaults while allowing command-line overrides.
set(CMAKE_CXX_FLAGS_RELEASE "-O2" CACHE STRING "Release compiler flags")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g" CACHE STRING "Debug compiler flags")

project(mangoDB LANGUAGES CXX)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format-22 clang-format)
if(NOT CLANG_FORMAT_EXECUTABLE)
    set(CLANG_FORMAT_EXECUTABLE clang-format)
endif()
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-22 clang-tidy)
if(NOT CLANG_TIDY_EXECUTABLE)
    set(CLANG_TIDY_EXECUTABLE clang-tidy)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE CHECK_FILES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h")
if(NOT SOURCES)
    message(FATAL_ERROR "No C++ source files found in src")
endif()

function(configure_mangoDB_target target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_options(${target} PRIVATE
        -Werror -Wall -Wextra -Wconversion -Wvla
        -Wno-unused-result -Wpedantic -pthread
    )
    target_link_options(${target} PRIVATE -pthread)
endfunction()

add_executable(mangoDB ${SOURCES})
configure_mangoDB_target(mangoDB)

add_executable(mangoDB-val EXCLUDE_FROM_ALL ${SOURCES})
configure_mangoDB_target(mangoDB-val)
target_compile_options(mangoDB-val PRIVATE -O0 -g -fno-omit-frame-pointer)

function(add_sanitizer_executable name sanitizer)
    add_executable(mangoDB-${name} EXCLUDE_FROM_ALL ${SOURCES})
    set_target_properties(mangoDB-${name} PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
    configure_mangoDB_target(mangoDB-${name})
    set(sanitizer_flags -fsanitize=${sanitizer})
    if(sanitizer STREQUAL "undefined")
        list(APPEND sanitizer_flags -fno-sanitize-recover=undefined)
    endif()
    target_compile_options(mangoDB-${name} PRIVATE
        -O1 -g -fno-omit-frame-pointer ${sanitizer_flags}
    )
    target_link_options(mangoDB-${name} PRIVATE ${sanitizer_flags})
endfunction()

add_sanitizer_executable(asan address)
add_sanitizer_executable(msan memory)
add_sanitizer_executable(ubsan undefined)

set(MANGODB_MSAN_LIBCXX_ROOT "" CACHE PATH "MSan-instrumented libc++ root")
if(NOT MANGODB_MSAN_LIBCXX_ROOT AND EXISTS "/usr/local/lib/libc++_msan-22")
    set(MANGODB_MSAN_LIBCXX_ROOT "/usr/local/lib/libc++_msan-22")
endif()

if(MANGODB_MSAN_LIBCXX_ROOT)
    foreach(required_path
            "include/c++/v1/sstream"
            "lib/libc++.so"
            "lib/libc++abi.so")
        if(NOT EXISTS "${MANGODB_MSAN_LIBCXX_ROOT}/${required_path}")
            message(FATAL_ERROR
                "Incomplete MSan libc++: missing ${MANGODB_MSAN_LIBCXX_ROOT}/${required_path}")
        endif()
    endforeach()

    target_compile_options(mangoDB-msan PRIVATE
        -nostdinc++ -isystem "${MANGODB_MSAN_LIBCXX_ROOT}/include/c++/v1"
    )
    target_link_options(mangoDB-msan PRIVATE -stdlib=libc++)
    target_link_directories(mangoDB-msan PRIVATE "${MANGODB_MSAN_LIBCXX_ROOT}/lib")
    set_property(TARGET mangoDB-msan APPEND PROPERTY BUILD_RPATH
        "${MANGODB_MSAN_LIBCXX_ROOT}/lib")
    message(STATUS "MSan uses instrumented libc++ from ${MANGODB_MSAN_LIBCXX_ROOT}")
else()
    message(STATUS "MSan uses the compiler's default C++ standard library")
endif()

add_custom_target(sanitizers
    DEPENDS mangoDB-asan mangoDB-msan mangoDB-ubsan
)

add_custom_target(run
    COMMAND $<TARGET_FILE:mangoDB> --listen /dev/stdin
    DEPENDS mangoDB
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXECUTABLE} --style=file -i ${CHECK_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Formatting C++ sources and headers"
    VERBATIM
)

add_custom_target(check-clang-format
    COMMAND ${CLANG_FORMAT_EXECUTABLE} --style=file --dry-run --Werror ${CHECK_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Checking C++ formatting"
    VERBATIM
)

add_custom_target(check-clang-tidy
    COMMAND ${CLANG_TIDY_EXECUTABLE} ${SOURCES}
        -p "${CMAKE_CURRENT_BINARY_DIR}"
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Running clang-tidy"
    VERBATIM
)

add_custom_target(check-cppcheck
    COMMAND cppcheck --enable=warning,style,performance,portability
        --std=c++20 --language=c++ --error-exitcode=1 --check-level=exhaustive
        "-I${CMAKE_CURRENT_SOURCE_DIR}/src" ${SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Running cppcheck"
    VERBATIM
)

add_custom_target(check-cpplint
    COMMAND cpplint --repository=${CMAKE_CURRENT_SOURCE_DIR}
        ${CHECK_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Running cpplint"
    VERBATIM
)

add_custom_target(check
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target check-clang-format
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target check-clang-tidy
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target check-cppcheck
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target check-cpplint
    COMMENT "Running all code quality checks"
    VERBATIM
)
