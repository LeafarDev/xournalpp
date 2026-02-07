/*
 * Xournal++
 *
 * LaTeX parser for chat responses
 *
 * @license GNU GPLv2 or later
 */

#include "latex/LatexParser.h"

#include <cstddef>

namespace xoj::latex {
namespace {
bool startsWithAt(const std::string& text, size_t pos, const std::string& needle) {
    return pos + needle.size() <= text.size() && text.compare(pos, needle.size(), needle) == 0;
}

size_t findUnescaped(const std::string& text, size_t start, char needle) {
    for (size_t i = start; i < text.size(); ++i) {
        if (text[i] == '\\') {
            ++i;
            continue;
        }
        if (text[i] == needle) {
            return i;
        }
    }
    return std::string::npos;
}
}

std::vector<Segment> LatexParser::parse(const std::string& input) {
    std::vector<Segment> segments;
    size_t i = 0;

    auto pushText = [&](const std::string& text) {
        if (!text.empty()) {
            segments.push_back({Segment::Type::TEXT, text});
        }
    };

    while (i < input.size()) {
        if (startsWithAt(input, i, "```latex") || startsWithAt(input, i, "```tex")) {
            size_t fenceStart = input.find('\n', i);
            if (fenceStart == std::string::npos) {
                pushText(input.substr(i));
                break;
            }
            size_t fenceEnd = input.find("```", fenceStart + 1);
            if (fenceEnd == std::string::npos) {
                pushText(input.substr(i));
                break;
            }
            std::string content = input.substr(fenceStart + 1, fenceEnd - fenceStart - 1);
            segments.push_back({Segment::Type::LATEX_BLOCK, content});
            i = fenceEnd + 3;
            continue;
        }

        if (startsWithAt(input, i, "$$")) {
            size_t end = input.find("$$", i + 2);
            if (end == std::string::npos) {
                pushText(input.substr(i));
                break;
            }
            std::string content = input.substr(i + 2, end - (i + 2));
            segments.push_back({Segment::Type::LATEX_BLOCK, content});
            i = end + 2;
            continue;
        }

        if (startsWithAt(input, i, "\\[")) {
            size_t end = input.find("\\]", i + 2);
            if (end != std::string::npos) {
                std::string content = input.substr(i + 2, end - (i + 2));
                segments.push_back({Segment::Type::LATEX_BLOCK, content});
                i = end + 2;
                continue;
            }
        }

        if (startsWithAt(input, i, "\\(")) {
            size_t end = input.find("\\)", i + 2);
            if (end != std::string::npos) {
                std::string content = input.substr(i + 2, end - (i + 2));
                segments.push_back({Segment::Type::LATEX_INLINE, content});
                i = end + 2;
                continue;
            }
        }

        if (input[i] == '$') {
            if (startsWithAt(input, i, "$$")) {
                // handled above
            } else {
                size_t end = findUnescaped(input, i + 1, '$');
                if (end != std::string::npos) {
                    std::string content = input.substr(i + 1, end - (i + 1));
                    segments.push_back({Segment::Type::LATEX_INLINE, content});
                    i = end + 1;
                    continue;
                }
            }
        }

        size_t next = input.size();
        size_t nextInline = input.find('$', i);
        size_t nextOpenParen = input.find("\\(", i);
        size_t nextOpenBracket = input.find("\\[", i);
        size_t nextFence = input.find("```latex", i);
        size_t nextFenceTex = input.find("```tex", i);
        size_t nextBlock = input.find("$$", i);

        if (nextInline != std::string::npos) next = std::min(next, nextInline);
        if (nextOpenParen != std::string::npos) next = std::min(next, nextOpenParen);
        if (nextOpenBracket != std::string::npos) next = std::min(next, nextOpenBracket);
        if (nextFence != std::string::npos) next = std::min(next, nextFence);
        if (nextFenceTex != std::string::npos) next = std::min(next, nextFenceTex);
        if (nextBlock != std::string::npos) next = std::min(next, nextBlock);

        if (next == i) {
            pushText(std::string(1, input[i]));
            ++i;
        } else {
            pushText(input.substr(i, next - i));
            i = next;
        }
    }

    return segments;
}

}  // namespace xoj::latex
