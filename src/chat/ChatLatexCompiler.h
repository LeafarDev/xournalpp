/*
 * Xournal++
 *
 * Compile chat LaTeX to PDF (minimal template, cache, worker-safe).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <variant>

#include "filesystem.h"

class LatexSettings;

namespace xoj::chat {

using CompileResult = std::variant<fs::path, std::string>;

/** Compile LaTeX to PDF; uses chat cache and minimal template. Safe to call from worker thread. */
CompileResult compileToPdf(const LatexSettings& settings, const std::string& latex, bool block);

}  // namespace xoj::chat
