cmake_minimum_required(VERSION 4.0)

project(image-color-analyzer
    VERSION 1.0.0.0
    DESCRIPTION "A small GUI tool to analyze the main colors of a given image"
    LANGUAGES CXX
)

if(WIN32)
    enable_language(RC)
    if(MSVC)
        # clang-cl forces us use "-" instead of "/" because it always
        # regard everything begins with "/" as a file path instead of
        # a command line parameter.
        set(CMAKE_RC_FLAGS "-c65001 -DWIN32 -nologo")
    endif()
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(LINUX AND NOT CMAKE_CROSSCOMPILING)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    if(EMSCRIPTEN)
        # Emscripten actually supports LTO, however, the link time is
        # too long, eg, many hours, you surely don't want to enable it.
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    else()
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(qmsetup QUIET)
if(NOT TARGET qmsetup::library)
    set(_qms_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/qmsetup")
    include("${_qms_source_dir}/cmake/modules/private/InstallPackage.cmake")
    set(_qms_package_path "")
    qm_install_package(qmsetup
        SOURCE_DIR "${_qms_source_dir}"
        BUILD_TYPE "Release"
        RESULT_PATH _qms_package_path
    )
    find_package(qmsetup REQUIRED PATHS ${_qms_package_path})
    set(qmsetup_DIR ${_qms_package_path} CACHE PATH "" FORCE)
endif()

qm_import(Filesystem CompilerOptions Preprocess Deploy)
qm_init_directories()
#qm_compiler_no_warnings()
#qm_compiler_max_warnings()
qm_compiler_warnings_are_errors()
qm_compiler_eliminate_dead_code()
qm_compiler_dont_export_by_default()

set(__branch "")
set(__hash "")
# We do not want to use git command here because we don't want to make git a build-time dependency.
if(EXISTS "${PROJECT_SOURCE_DIR}/.git/HEAD")
    file(READ "${PROJECT_SOURCE_DIR}/.git/HEAD" __hash)
    string(STRIP "${__hash}" __hash)
    if(__hash MATCHES "^ref: (.*)")
        set(HEAD "${CMAKE_MATCH_1}")
        string(REPLACE "refs/heads/" "" __branch "${HEAD}")
        if(EXISTS "${PROJECT_SOURCE_DIR}/.git/${HEAD}")
            file(READ "${PROJECT_SOURCE_DIR}/.git/${HEAD}" __hash)
            string(STRIP "${__hash}" __hash)
        else()
            file(READ "${PROJECT_SOURCE_DIR}/.git/packed-refs" PACKED_REFS)
            string(REGEX REPLACE ".*\n([0-9a-f]+) ${HEAD}\n.*" "\\1" __hash "\n${PACKED_REFS}")
        endif()
    endif()
endif()
set(__hash_short "")
string(SUBSTRING "${__hash}" 0 8 __hash_short)
if("x${__branch}" STREQUAL "x")
    message(WARNING "Failed to retrieve the branch information from git, using the default branch name `main` instead.")
    set(__branch "main")
endif()

message("-----------------------------------------------------------")
message("CMAKE_PROJECT_NAME: ${CMAKE_PROJECT_NAME}")
message("CMAKE_PROJECT_VERSION: ${CMAKE_PROJECT_VERSION}")
message("CMAKE_PROJECT_DESCRIPTION: ${CMAKE_PROJECT_DESCRIPTION}")
message("CMAKE_PROJECT_HOMEPAGE_URL: ${CMAKE_PROJECT_HOMEPAGE_URL}")
message("==================")
message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message("CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}")
message("CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message("==================")
message("CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message("CMAKE_CXX_COMPILER_TARGET: ${CMAKE_CXX_COMPILER_TARGET}")
message("CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message("CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message("CMAKE_LINKER: ${CMAKE_LINKER}")
message("==================")
message("CMAKE_COMMAND: ${CMAKE_COMMAND}")
message("CMAKE_VERSION: ${CMAKE_VERSION}")
message("CMAKE_MAKE_PROGRAM: ${CMAKE_MAKE_PROGRAM}")
message("==================")
message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("==================")
message("CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message("CMAKE_LIBRARY_OUTPUT_DIRECTORY: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message("CMAKE_ARCHIVE_OUTPUT_DIRECTORY: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
message("==================")
message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("CMAKE_STAGING_PREFIX: ${CMAKE_STAGING_PREFIX}")
message("==================")
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("CMAKE_CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
message("CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}")
message("CMAKE_CROSSCOMPILING: ${CMAKE_CROSSCOMPILING}")
message("==================")
message("Branch: ${__branch}")
message("Commit: ${__hash}")
message("-----------------------------------------------------------")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE mainwindow.h mainwindow.cpp main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

qt_add_resources(${PROJECT_NAME} "resources"
    PREFIX "/"
    FILES
        "fonts/JetBrainsMono-Regular.ttf"
        "fonts/JetBrainsMono-Bold.ttf"
        "fonts/JetBrainsMono-Italic.ttf"
        "fonts/JetBrainsMono-BoldItalic.ttf"
)

qm_compiler_enable_strict_qt(TARGETS ${PROJECT_NAME} NO_DEPRECATED_API)

if(WIN32)
    qm_add_win_manifest(${PROJECT_NAME} UTF8
        NAME "${PROJECT_NAME}"
        VERSION "${PROJECT_VERSION}"
        DESCRIPTION "${PROJECT_DESCRIPTION}"
    )
    qm_add_win_rc_enhanced(${PROJECT_NAME}
        NAME "${PROJECT_NAME}"
        VERSION "${PROJECT_VERSION}"
        DESCRIPTION "${PROJECT_DESCRIPTION}"
        COMPANY "wangwenx190"
        COPYRIGHT "The MIT License"
        COMMENTS "Based on the advanced Qt & modern C++ technology."
        ICONS "${PROJECT_SOURCE_DIR}/icons/palette.ico"
    )
    qm_win_record_deps(${PROJECT_NAME})
    qm_win_applocal_deps(${PROJECT_NAME} VERBOSE EXCLUDE "Qt\\d+\\w+\\.dll")
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "wangwenx190")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "${__package_base_name}") # Must not contains the file extension name!
set(CPACK_PACKAGE_CHECKSUM "SHA512") # MD5, SHA1, SHA256, SHA512
set(CPACK_GENERATOR "7Z")
set(CPACK_THREADS 0) # 0 means use all available CPU cores.
# This must always be after all CPACK_* variables are defined
include(CPack)
