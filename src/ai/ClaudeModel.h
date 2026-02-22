/*
 * Xournal++
 *
 * Anthropic Claude API-backed chat model.
 *
 * Streams responses from the Anthropic Messages API using curl as a subprocess,
 * parsing Server-Sent Events to extract text_delta chunks.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>

#include "ai/IChatModel.h"

class ClaudeModel: public IChatModel {
public:
    /// @param apiKey  Anthropic API key. If empty, falls back to ANTHROPIC_API_KEY env var.
    explicit ClaudeModel(std::string apiKey = {});
    ~ClaudeModel() override = default;

    const Params& params() const override { return p; }

    void sendMessage(const MessageList& history,
                     std::function<void(const std::string&)> onToken,
                     std::function<void()> onComplete,
                     std::function<void(const std::string&)> onError) override;

    void cancel() override;

private:
    Params p;
    std::string storedApiKey;
    std::atomic<bool> cancelled{false};
};
