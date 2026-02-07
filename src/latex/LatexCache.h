/*
 * Xournal++
 *
 * LaTeX cache helper
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

#include "filesystem.h"

namespace xoj::latex {

class LatexCache {
public:
    static fs::path getCacheDir();
    static fs::path pathFor(const std::string& latex, bool block);
};

}  // namespace xoj::latex
