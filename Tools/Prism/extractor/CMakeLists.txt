set(PRISM_EXTRACT_SOURCES
    src/main.cpp
    src/Support/PathUtils.cpp
    src/Support/IdGen.cpp
    src/Support/CommentParser.cpp
    src/AST/DeclExtractor.cpp
    src/AST/RelationshipCollector.cpp
    src/Emit/JsonWriter.cpp
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

# ─── Clang / LLVM Libraries ───────────────────────────────────────────────────
#
# We link the superset required for a full frontend parse.
# On platforms using LLVM shared libraries this resolves to a single dylib;
# on static-library builds every archive is listed explicitly.

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

# ─── Warning Levels ───────────────────────────────────────────────────────────

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