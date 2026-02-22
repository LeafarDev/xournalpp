/*
 * Xournal++
 *
 * Local llama.cpp-backed chat model.
 *
 * Uses the existing LLMEngine wrapper (blocking for now; can be extended to
 * streaming later).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>
#include <string>

#include "ai/IChatModel.h"
#include "ai/LLMEngine.h"

class LocalLlamaModel: public IChatModel {
public:
    explicit LocalLlamaModel(const std::string& modelPath);
    ~LocalLlamaModel() override = default;

    const Params& params() const override { return p; }

    void sendMessage(const MessageList& history,
                     std::function<void(const std::string&)> onToken,
                     std::function<void()> onComplete,
                     std::function<void(const std::string&)> onError) override;

    void cancel() override;

private:
    Params p;
    LLMEngine engine;
    std::atomic<bool> cancelled{false};
    bool ready = false;
};

