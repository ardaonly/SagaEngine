cmake_minimum_required(VERSION 3.22)
project(PrismExtract CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── ZLIB Detection (NixOS-safe) ──────────────────────────────────────

find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ZLIB_PKG QUIET zlib)
    endif()

    if(ZLIB_PKG_FOUND)
        find_library(ZLIB_LIBRARY
            NAMES z zlib
            PATHS ${ZLIB_PKG_LIBRARY_DIRS}
            NO_DEFAULT_PATH
        )
        find_path(ZLIB_INCLUDE_DIR
            NAMES zlib.h
            PATHS ${ZLIB_PKG_INCLUDE_DIRS}
            NO_DEFAULT_PATH
        )
    endif()

    if(NOT ZLIB_LIBRARY OR NOT ZLIB_INCLUDE_DIR)
        find_library(ZLIB_LIBRARY NAMES z zlib PATHS /nix/store NO_DEFAULT_PATH)
        find_path(ZLIB_INCLUDE_DIR NAMES zlib.h PATHS /nix/store NO_DEFAULT_PATH)
    endif()

    if(ZLIB_LIBRARY AND ZLIB_INCLUDE_DIR)
        set(ZLIB_FOUND TRUE)
        set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
        set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
    endif()
endif()

if(ZLIB_FOUND AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LOCATION "${ZLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}"
    )
    message(STATUS "Created ZLIB::ZLIB target: ${ZLIB_LIBRARY}")
endif()

# ─── LibXml2 Detection (NixOS-safe) ───────────────────────────────────

find_package(LibXml2 QUIET)

if(NOT LIBXML2_FOUND)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBXML2_PKG QUIET libxml-2.0)
    endif()

    if(LIBXML2_PKG_FOUND)
        find_library(LIBXML2_LIBRARY
            NAMES xml2 libxml2
            PATHS ${LIBXML2_PKG_LIBRARY_DIRS}
            NO_DEFAULT_PATH
        )
        find_path(LIBXML2_INCLUDE_DIR
            NAMES libxml2/libxml/xmlexports.h
            PATHS ${LIBXML2_PKG_INCLUDE_DIRS}
            NO_DEFAULT_PATH
        )
    endif()

    if(NOT LIBXML2_LIBRARY OR NOT LIBXML2_INCLUDE_DIR)
        find_library(LIBXML2_LIBRARY NAMES xml2 libxml2 PATHS /nix/store NO_DEFAULT_PATH)
        find_path(LIBXML2_INCLUDE_DIR NAMES libxml2/libxml/xmlexports.h libxml/xmlexports.h PATHS /nix/store NO_DEFAULT_PATH)
    endif()

    if(LIBXML2_LIBRARY AND LIBXML2_INCLUDE_DIR)
        set(LIBXML2_FOUND TRUE)
        set(LIBXML2_LIBRARIES ${LIBXML2_LIBRARY})
        set(LIBXML2_INCLUDE_DIRS ${LIBXML2_INCLUDE_DIR})
    endif()
endif()

if(LIBXML2_FOUND AND NOT TARGET LibXml2::LibXml2)
    add_library(LibXml2::LibXml2 UNKNOWN IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
        IMPORTED_LOCATION "${LIBXML2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
    )
    message(STATUS "Created LibXml2::LibXml2 target: ${LIBXML2_LIBRARY}")
endif()

# ─── LLVM / Clang Setup ───────────────────────────────────────────────

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")

find_package(Clang REQUIRED CONFIG)
message(STATUS "Found Clang in ${Clang_DIR}")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ─── Sources ───────────────────────────────────────────────────────────

set(PRISM_EXTRACT_SOURCES
    src/main.cpp
    src/Support/PathUtils.cpp
    src/Support/IdGen.cpp
    src/Support/CommentParser.cpp
    src/AST/DeclExtractor.cpp
    src/AST/RelationshipCollector.cpp
    src/AST/SymbolVisitor.cpp
    src/Pipeline/Emit/JsonEmitter.cpp
    src/Pipeline/PrismConsumer.cpp
    src/Pipeline/PrismAction.cpp
)

add_executable(prism-extract ${PRISM_EXTRACT_SOURCES})

target_include_directories(prism-extract
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LLVM_INCLUDE_DIRS}
        ${CLANG_INCLUDE_DIRS}
)

# ─── Clang / LLVM Libraries ───────────────────────────────────────────

llvm_map_components_to_libnames(LLVM_LIBS support core)

target_link_libraries(prism-extract
    PRIVATE
        clangTooling
        clangToolingCore
        clangAST
        clangASTMatchers
        clangBasic
        clangFrontend
        clangFrontendTool
        clangSerialization
        clangDriver
        clangParse
        clangSema
        clangAnalysis
        clangEdit
        clangLex
        clangIndex
        ${LLVM_LIBS}
)

target_compile_definitions(prism-extract PRIVATE ${LLVM_DEFINITIONS})

# ─── Warning Levels ───────────────────────────────────────────────────

if(MSVC)
    target_compile_options(prism-extract PRIVATE /W4 /WX /wd4100 /wd4996)
else()
    target_compile_options(prism-extract PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wcast-align -Wunused
        -Wno-unused-parameter
    )
endif()

install(TARGETS prism-extract RUNTIME DESTINATION bin)
