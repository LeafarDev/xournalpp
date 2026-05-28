/*
 * Xournal++
 *
 * GitHub Copilot CLI-backed chat model.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/CopilotCliModel.h"

#include <sstream>
#include <string>
#include <thread>

#include <gio/gio.h>

#include <glib.h>

#include "util/PathUtil.h"
#include "util/StringUtils.h"

CopilotCliModel::CopilotCliModel() {
    p.id = "copilot";
    p.displayName = "GitHub Copilot (CLI)";
    // Conservative default; Copilot may support more but this is sufficient
    // for a first implementation.
    p.maxContextTokens = 8192;
}

void CopilotCliModel::sendMessage(const MessageList& history,
                                  std::function<void(const std::string&)> onToken,
                                  std::function<void()> onComplete,
                                  std::function<void(const std::string&)> onError) {
    if (!onToken || !onComplete || !onError) {
        return;
    }

    auto copilotPath = Util::getBundledCopilotPath();
    if (copilotPath.empty() || !fs::exists(copilotPath) || !fs::is_regular_file(copilotPath)) {
        onError("Copilot CLI not found in bundle. Rebuild with bundle-copilot.sh.");
        return;
    }

    // Reconstruct a single prompt from the message history. Copilot CLI does
    // not natively support a role-annotated conversation, so we serialize the
    // roles explicitly into text.
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
    oss << "[Assistant]\n";
    const std::string prompt = oss.str();

    cancelled.store(false);

    std::thread([this, copilotPathStr = copilotPath.string(), prompt, onToken, onComplete,
                 onError]() mutable {
      try {
        GError* err = nullptr;
        // Use STDIN_PIPE so we can immediately close stdin (send EOF).
        // This prevents the CLI from waiting for interactive confirmations,
        // keeping it in default chat-only mode and avoiding premium agent quota.
        // We do NOT pass --allow-all so agentic tool execution is not enabled.
        GSubprocess* proc = g_subprocess_new(
                static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                              G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                              G_SUBPROCESS_FLAGS_STDERR_PIPE),
                &err, copilotPathStr.c_str(), "-p", prompt.c_str(), "-s", nullptr);
        if (!proc) {
            std::string msg = err ? err->message : "Failed to start Copilot CLI.";
            if (err) {
                g_error_free(err);
            }
            g_debug("[chat] Copilot: failed to start subprocess: %s", msg.c_str());
            onError("Erro ao iniciar Copilot: " + msg);
            return;
        }
        g_debug("[chat] Copilot: subprocess started, reading stdout...");

        // Close stdin immediately — sends EOF to the CLI so it can't block
        // waiting for tool-use confirmations. Responses come via stdout only.
        {
            GOutputStream* stdinPipe = g_subprocess_get_stdin_pipe(proc);
            if (stdinPipe) {
                g_output_stream_close(stdinPipe, nullptr, nullptr);
            }
        }

        GInputStream* outStream = g_subprocess_get_stdout_pipe(proc);
        GInputStream* errStream = g_subprocess_get_stderr_pipe(proc);
        if (!outStream) {
            g_object_unref(proc);
            onError("Copilot CLI não forneceu stdout.");
            return;
        }

        char buf[4096];
        std::string accumulated;
        std::string errorAccumulated;
        
        // Read stderr in parallel to capture errors
        std::thread errReader([errStream, &errorAccumulated, this]() {
            char errBuf[4096];
            while (!this->cancelled.load()) {
                GError* readErr = nullptr;
                gssize n = g_input_stream_read(errStream, errBuf, sizeof(errBuf), nullptr, &readErr);
                if (n <= 0) {
                    if (readErr) {
                        g_error_free(readErr);
                    }
                    break;
                }
                errorAccumulated.append(errBuf, static_cast<std::size_t>(n));
            }
        });
        
        int readCount = 0;
        while (!cancelled.load()) {
            GError* readErr = nullptr;
            gssize n = g_input_stream_read(outStream, buf, sizeof(buf), nullptr, &readErr);
            if (n <= 0) {
                if (readErr) {
                    std::string errMsg = readErr->message ? readErr->message : "Erro ao ler stdout";
                    g_error_free(readErr);
                    g_debug("[chat] Copilot: read error: %s", errMsg.c_str());
                    errReader.join();
                    g_subprocess_wait(proc, nullptr, nullptr);
                    g_object_unref(proc);
                    onError("Erro ao ler resposta do Copilot: " + errMsg);
                    return;
                }
                g_debug("[chat] Copilot: stdout EOF after %d reads, %zu bytes total", readCount, accumulated.size());
                break;
            }
            if (readCount == 0) {
                g_debug("[chat] Copilot: first chunk received (%zd bytes)", n);
            }
            readCount++;
            accumulated.append(buf, static_cast<std::size_t>(n));
            onToken(std::string(buf, static_cast<std::size_t>(n)));
        }

        errReader.join();
        
        g_subprocess_wait(proc, nullptr, nullptr);
        gint exitStatus = g_subprocess_get_exit_status(proc);
        g_debug("[chat] Copilot: exit status=%d, stderr=%zu chars", exitStatus,
                static_cast<size_t>(errorAccumulated.size()));

        // Check exit status and stderr for errors
        if (exitStatus != 0 || !errorAccumulated.empty()) {
            std::string errorMsg = "Copilot retornou erro";
            if (exitStatus != 0) {
                errorMsg += " (código: " + std::to_string(exitStatus) + ")";
            }
            if (!errorAccumulated.empty()) {
                errorMsg += ": " + StringUtils::trim(errorAccumulated);
            }
            g_debug("[chat] Copilot: calling onError: %s", errorMsg.c_str());
            g_object_unref(proc);
            onError(errorMsg);
            return;
        }
        
        g_object_unref(proc);

        if (cancelled.load()) {
            // Silent cancel.
            return;
        }

        std::string trimmed = StringUtils::trim(accumulated);
        if (trimmed.empty()) {
            g_debug("[chat] Copilot: output empty, calling onError");
            onError("Copilot não retornou texto. Verifique 'copilot login' e assinatura.");
            return;
        }

        g_debug("[chat] Copilot: got %zu chars total, calling onComplete", accumulated.size());
        onComplete();
      } catch (const std::exception& e) {
        g_warning("[chat] Copilot: unhandled exception: %s", e.what());
        try { onError(std::string("Copilot: unexpected error — ") + e.what()); } catch (...) {}
      } catch (...) {
        g_warning("[chat] Copilot: unknown unhandled exception");
        try { onError("Copilot: unknown error occurred."); } catch (...) {}
      }
    }).detach();
}

void CopilotCliModel::cancel() {
    cancelled.store(true);
    // The reader loop will observe this and stop; the subprocess will be
    // allowed to exit naturally. If this proves too slow, we can wire an
    // explicit GSubprocess handle and call g_subprocess_force_exit().
}

