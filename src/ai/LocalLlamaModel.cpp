/*
 * Xournal++
 *
 * Local llama.cpp-backed chat model.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/LocalLlamaModel.h"

#include <sstream>
#include <thread>

#include <glib.h>

LocalLlamaModel::LocalLlamaModel(const std::string& modelPath) {
    p.id = "local-llama";
    p.displayName = "Local LLaMA";
    // Initial guess; the underlying engine currently hardcodes 2048 in LLMEngine.
    p.maxContextTokens = 2048;

    ready = engine.init(modelPath);
}

void LocalLlamaModel::sendMessage(const MessageList& history,
                                  std::function<void(const std::string&)> onToken,
                                  std::function<void()> onComplete,
                                  std::function<void(const std::string&)> onError) {
    if (!onToken || !onComplete || !onError) {
        return;
    }

    if (!ready) {
        onError("Modelo local não está disponível. Verifique XOURNALPP_LLM_MODEL e o caminho do modelo.");
        return;
    }

    // Build a simple prompt combining all messages.
    std::ostringstream oss;
    for (const auto& m: history) {
        if (m.role == "system") {
            oss << "[System]\n" << m.content << "\n\n";
        } else if (m.role == "user") {
            oss << "[User]\n" << m.content << "\n\n";
        } else {
            oss << "[Assistant]\n" << m.content << "\n\n";
        }
    }
    // Make it explicit that the next text should be from the assistant.
    oss << "[Assistant]\n";
    const std::string prompt = oss.str();

    cancelled.store(false);

    std::thread([this, prompt, onToken, onComplete, onError]() {
      try {
        // Blocking run for now; when LLMEngine gains a streaming API this
        // can be replaced by incremental token callbacks.
        std::string output = engine.run(prompt);

        if (cancelled.load()) {
            // Silent cancellation.
            return;
        }

        if (output.empty()) {
            g_debug("[chat] LocalLlama: output empty, calling onError");
            onError("Modelo local não retornou saída. Verifique se o modelo está carregado corretamente.");
            return;
        }

        g_debug("[chat] LocalLlama: got %zu chars, calling onToken + onComplete", output.size());
        onToken(output);
        onComplete();
      } catch (const std::exception& e) {
        g_warning("[chat] LocalLlama: unhandled exception: %s", e.what());
        try { onError(std::string("Local model: unexpected error — ") + e.what()); } catch (...) {}
      } catch (...) {
        g_warning("[chat] LocalLlama: unknown unhandled exception");
        try { onError("Local model: unknown error occurred."); } catch (...) {}
      }
    }).detach();
}

void LocalLlamaModel::cancel() {
    cancelled.store(true);
    // LLMEngine currently has no explicit cancel primitive; once streaming is
    // implemented, we can plumb this flag through the decode loop.
}

