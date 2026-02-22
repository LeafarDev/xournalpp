/*
 * Xournal++
 *
 * Token‑aware chat context builder.
 *
 * Responsible for assembling a sliding window of messages (system + history)
 * that fits within a target model's approximate context size.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>
#include <string>

#include "ai/ChatTypes.h"

class ContextBuilder {
public:
    ContextBuilder() = default;

    void setSystemPrompt(std::string prompt);

    void addUserMessage(std::string text);
    void addAssistantMessage(std::string text);

    /**
     * Build a message list suitable for a model with a context window of
     * @p maxTokens tokens (approximate).
     *
     * Strategy:
     *  - Always include the system prompt if present.
     *  - Walk history from newest to oldest, estimating tokens per message.
     *  - Stop when adding another message would exceed @p maxTokens.
     *  - Return messages in natural order: system (0 or 1), then oldest..newest.
     */
    MessageList buildForModel(std::size_t maxTokens) const;

    void clear();

    const MessageList& rawHistory() const { return history; }

private:
    std::string systemPrompt;
    MessageList history;  ///< chronological: oldest → newest

    static std::size_t estimateTokens(const std::string& text);
};

