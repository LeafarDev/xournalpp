/*
 * Xournal++
 *
 * Cache for chat LaTeX → PDF. One file per (hash of content + options).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

#include "filesystem.h"

namespace xoj::chat {

class ChatLatexCache {
public:
    static constexpr const char* TEMPLATE_VERSION = "1";

    static fs::path getCacheDir();
    /// Returns path to cached PDF; file may not exist yet.
    static fs::path pathFor(const std::string& latex, bool block);
};

}  // namespace xoj::chat
