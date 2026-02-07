/*
 * Xournal++
 *
 * LaTeX parser for chat responses
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <vector>

namespace xoj::latex {

struct Segment {
    enum class Type { TEXT, LATEX_INLINE, LATEX_BLOCK } type;
    std::string content;
};

class LatexParser {
public:
    static std::vector<Segment> parse(const std::string& input);
};

}  // namespace xoj::latex
