// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <filesystem>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

inline std::filesystem::path TestSourceRoot()
{
    const auto configured = std::filesystem::path(SAGA_SOURCE_ROOT);
    if (std::filesystem::is_regular_file(
            configured / "Samples/StarterArena/StarterArena.sagaproj"))
        return configured;

    auto cursor = std::filesystem::current_path();
    for (;;)
    {
        if (std::filesystem::is_regular_file(
                cursor / "Samples/StarterArena/StarterArena.sagaproj"))
            return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor)
            break;
        cursor = parent;
    }
    return configured;
}
