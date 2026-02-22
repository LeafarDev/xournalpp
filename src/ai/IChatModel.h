/*
 * Xournal++
 *
 * Abstract chat model interface.
 *
 * Backends (local llama.cpp, Copilot CLI, HTTP/OpenAI-compatible, etc.) must
 * implement this interface. All long-running work is expected to happen on
 * background threads; callbacks may be invoked from those threads and the
 * caller is responsible for marshaling to the UI thread if needed.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>
#include <string>

#include "ai/ChatTypes.h"

class IChatModel {
public:
    virtual ~IChatModel() = default;

    struct Params {
        std::string id;           ///< Stable identifier, e.g. "local-llama", "copilot"
        std::string displayName;  ///< Human‑readable name for UI
        std::size_t maxContextTokens = 0;  ///< Approximate usable context window
    };

    /**
     * Metadata describing this model instance.
     */
    virtual const Params& params() const = 0;

    /**
     * Run a chat completion with the given message history.
     *
     * @param history    System + user + assistant messages, already trimmed to the
     *                   model's context window by the caller.
     * @param onToken    Called for each chunk of assistant output. May be called
     *                   from a worker thread.
     * @param onComplete Called exactly once when generation finishes successfully.
     * @param onError    Called if an error occurs. No further callbacks will
     *                   follow after onError.
     */
    virtual void sendMessage(const MessageList& history,
                             std::function<void(const std::string&)> onToken,
                             std::function<void()> onComplete,
                             std::function<void(const std::string&)> onError) = 0;

    /**
     * Request cancellation of an in‑flight generation.
     *
     * Implementations should be best‑effort; they may not be able to abort
     * instantly, but should stop producing tokens as soon as practicable.
     */
    virtual void cancel() = 0;
};

