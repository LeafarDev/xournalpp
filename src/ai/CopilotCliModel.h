/*
 * Xournal++
 *
 * GitHub Copilot CLI-backed chat model.
 *
 * This implementation shells out to the bundled Copilot CLI binary and streams
 * its stdout back as assistant tokens.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>

#include "ai/IChatModel.h"

class CopilotCliModel: public IChatModel {
public:
    CopilotCliModel();
    ~CopilotCliModel() override = default;

    const Params& params() const override { return p; }

    void sendMessage(const MessageList& history,
                     std::function<void(const std::string&)> onToken,
                     std::function<void()> onComplete,
                     std::function<void(const std::string&)> onError) override;

    void cancel() override;

private:
    Params p;
    std::atomic<bool> cancelled{false};
};

