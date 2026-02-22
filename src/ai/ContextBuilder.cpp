/*
 * Xournal++
 *
 * Token‑aware chat context builder.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/ContextBuilder.h"

#include <algorithm>

void ContextBuilder::setSystemPrompt(std::string prompt) {
    systemPrompt = std::move(prompt);
}

void ContextBuilder::addUserMessage(std::string text) {
    history.push_back(Message{"user", std::move(text)});
}

void ContextBuilder::addAssistantMessage(std::string text) {
    history.push_back(Message{"assistant", std::move(text)});
}

void ContextBuilder::clear() {
    systemPrompt.clear();
    history.clear();
}

std::size_t ContextBuilder::estimateTokens(const std::string& text) {
    // Cheap heuristic: 4 characters per token as a rough average.
    // This can be replaced by a model‑specific tokenizer later.
    std::size_t len = text.size();
    return std::max<std::size_t>(std::size_t{1}, len / 4);
}

MessageList ContextBuilder::buildForModel(std::size_t maxTokens) const {
    MessageList result;
    std::size_t used = 0;

    // Reserve a bit of space to reduce reallocations for typical histories.
    result.reserve(history.size() + 1);

    if (!systemPrompt.empty()) {
        result.push_back(Message{"system", systemPrompt});
        used += estimateTokens(systemPrompt);
    }

    if (maxTokens == 0) {
        // Degenerate: just return system + full history.
        result.insert(result.end(), history.begin(), history.end());
        return result;
    }

    // Walk history from newest to oldest, inserting before the most recent
    // messages while staying under the token budget.
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        std::size_t t = estimateTokens(it->content);

        if (used + t > maxTokens && !result.empty()) {
            break;
        }

        // Insert just after the system message (if any), so order is:
        // [system], msg0, msg1, ..., msgN (chronological).
        auto insertPos = result.begin();
        if (!systemPrompt.empty()) {
            ++insertPos;
        }
        result.insert(insertPos, *it);
        used += t;
    }

    return result;
}

