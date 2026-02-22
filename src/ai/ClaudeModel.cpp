/*
 * Xournal++
 *
 * Anthropic Claude API-backed chat model.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/ClaudeModel.h"

#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gio/gio.h>
#include <glib.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Escape a string for embedding inside a JSON double-quoted value.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch: s) {
        unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    // Control character → \uXXXX
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

/// Unescape a JSON string value (used when extracting text from SSE data).
std::string jsonUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool esc = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (esc) {
            switch (c) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u':
                    // \uXXXX – decode basic BMP code point to UTF-8
                    if (i + 4 < s.size()) {
                        unsigned cp = 0;
                        for (size_t k = 1; k <= 4; ++k) {
                            char h = s[i + k];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        }
                        i += 4;
                        if (cp < 0x80) {
                            out += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                default: out += c; break;
            }
            esc = false;
        } else if (c == '\\') {
            esc = true;
        } else {
            out += c;
        }
    }
    return out;
}

/// Build the Anthropic Messages API JSON request body.
std::string buildRequestJson(const MessageList& history, const std::string& model) {
    // Split history into system content + conversation messages.
    std::string systemContent;
    std::vector<const Message*> conv;
    for (const auto& m: history) {
        if (m.role == "system") {
            if (!systemContent.empty()) {
                systemContent += "\n\n";
            }
            systemContent += m.content;
        } else {
            conv.push_back(&m);
        }
    }

    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << jsonEscape(model) << "\",";
    oss << "\"max_tokens\":8192,";
    oss << "\"stream\":true";

    if (!systemContent.empty()) {
        oss << ",\"system\":\"" << jsonEscape(systemContent) << "\"";
    }

    oss << ",\"messages\":[";
    bool first = true;
    for (const auto* m: conv) {
        if (!first) { oss << ","; }
        first = false;
        oss << "{\"role\":\"" << jsonEscape(m->role) << "\","
            << "\"content\":\"" << jsonEscape(m->content) << "\"}";
    }
    oss << "]}";

    return oss.str();
}

/// Extract the text value from a Claude SSE data line.
/// Expected shape: {"type":"content_block_delta","delta":{"type":"text_delta","text":"..."}}
std::string extractTextDelta(const std::string& data) {
    // Confirm this is a text_delta event.
    if (data.find("\"text_delta\"") == std::string::npos) {
        return "";
    }
    // Find "text":" after the text_delta marker.
    auto tdPos = data.find("\"text_delta\"");
    auto keyPos = data.find("\"text\":\"", tdPos);
    if (keyPos == std::string::npos) {
        return "";
    }
    keyPos += 8;  // skip past "text":"

    // Read the JSON string value, handling escape sequences.
    std::string raw;
    bool esc = false;
    for (size_t i = keyPos; i < data.size(); ++i) {
        char c = data[i];
        if (esc) {
            raw += '\\';
            raw += c;
            esc = false;
        } else if (c == '\\') {
            esc = true;
        } else if (c == '"') {
            break;
        } else {
            raw += c;
        }
    }
    return jsonUnescape(raw);
}

}  // namespace

// ---------------------------------------------------------------------------
// ClaudeModel
// ---------------------------------------------------------------------------

ClaudeModel::ClaudeModel(std::string apiKey): storedApiKey(std::move(apiKey)) {
    p.id = "claude";
    p.displayName = "Claude (Anthropic)";
    p.maxContextTokens = 180000;  // claude-sonnet-4-6 / opus-4-6 context window
}

void ClaudeModel::sendMessage(const MessageList& history,
                              std::function<void(const std::string&)> onToken,
                              std::function<void()> onComplete,
                              std::function<void(const std::string&)> onError) {
    if (!onToken || !onComplete || !onError) {
        return;
    }

    // Resolve API key: prefer the value stored in Settings, fall back to env var.
    std::string apiKey = storedApiKey;
    g_debug("[chat] Claude: storedApiKey length=%zu", storedApiKey.size());
    if (apiKey.empty()) {
        const char* envKey = g_getenv("ANTHROPIC_API_KEY");
        if (envKey && envKey[0]) {
            apiKey = envKey;
            g_debug("[chat] Claude: using ANTHROPIC_API_KEY from environment");
        }
    }
    if (apiKey.empty()) {
        g_debug("[chat] Claude: no API key available, calling onError");
        onError("No Anthropic API key set. Select Claude in the model combo — an 'API Key' field "
                "appears below. Paste your key from console.anthropic.com and press Enter.");
        return;
    }

    // Allow override of model via ANTHROPIC_MODEL env var.
    const char* modelEnv = g_getenv("ANTHROPIC_MODEL");
    std::string model = (modelEnv && modelEnv[0]) ? modelEnv : "claude-sonnet-4-6";

    std::string body = buildRequestJson(history, model);
    std::string authHeader = "x-api-key: " + apiKey;
    const std::string versionHeader = "anthropic-version: 2023-06-01";
    const std::string ctHeader = "content-type: application/json";

    g_debug("[chat] Claude: sending request to Anthropic API, model=%s, body=%zu bytes",
            model.c_str(), body.size());

    cancelled.store(false);

    std::thread([this, body, authHeader, versionHeader, ctHeader, onToken, onComplete,
                 onError]() mutable {
      try {
        GError* err = nullptr;
        GSubprocess* proc = g_subprocess_new(
                static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                              G_SUBPROCESS_FLAGS_STDERR_PIPE),
                &err, "curl",
                "-s",               // silent (no progress bar)
                "-N",               // no buffering (important for streaming)
                "-X", "POST",
                "https://api.anthropic.com/v1/messages",
                "-H", ctHeader.c_str(),
                "-H", authHeader.c_str(),
                "-H", versionHeader.c_str(),
                "-d", body.c_str(),
                nullptr);

        if (!proc) {
            std::string msg = err ? err->message : "Failed to start curl.";
            if (err) { g_error_free(err); }
            g_debug("[chat] Claude: failed to start subprocess: %s", msg.c_str());
            onError("Erro ao iniciar curl: " + msg);
            return;
        }

        GInputStream* outStream = g_subprocess_get_stdout_pipe(proc);
        GInputStream* errStream = g_subprocess_get_stderr_pipe(proc);

        // Collect stderr in a background thread.
        std::string stderrBuf;
        std::thread errReader([errStream, &stderrBuf, this]() {
            char buf[4096];
            while (!this->cancelled.load()) {
                GError* re = nullptr;
                gssize n = g_input_stream_read(errStream, buf, sizeof(buf), nullptr, &re);
                if (n <= 0) {
                    if (re) { g_error_free(re); }
                    break;
                }
                stderrBuf.append(buf, static_cast<size_t>(n));
            }
        });

        // Read stdout and parse SSE.
        std::string lineBuf;
        char buf[4096];
        bool done = false;

        while (!cancelled.load() && !done) {
            GError* re = nullptr;
            gssize n = g_input_stream_read(outStream, buf, sizeof(buf), nullptr, &re);
            if (n <= 0) {
                if (re) { g_error_free(re); }
                break;
            }
            lineBuf.append(buf, static_cast<size_t>(n));

            // Process all complete lines in the buffer.
            size_t pos;
            while ((pos = lineBuf.find('\n')) != std::string::npos) {
                std::string line = lineBuf.substr(0, pos);
                lineBuf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (line.rfind("data: ", 0) == 0) {
                    std::string data = line.substr(6);
                    if (data == "[DONE]") {
                        done = true;
                        break;
                    }
                    // Check for API-level error in the JSON.
                    if (data.find("\"error\"") != std::string::npos &&
                        data.find("\"message\"") != std::string::npos) {
                        // Extract error message for display.
                        auto mPos = data.find("\"message\":\"");
                        if (mPos != std::string::npos) {
                            mPos += 11;
                            std::string errMsg;
                            for (size_t i = mPos; i < data.size() && data[i] != '"'; ++i) {
                                errMsg += data[i];
                            }
                            errReader.join();
                            g_subprocess_wait(proc, nullptr, nullptr);
                            g_object_unref(proc);
                            onError("Claude API error: " + errMsg);
                            return;
                        }
                    }
                    std::string text = extractTextDelta(data);
                    if (!text.empty() && !cancelled.load()) {
                        onToken(text);
                    }
                }
            }
        }

        errReader.join();
        g_subprocess_wait(proc, nullptr, nullptr);
        gint exitStatus = g_subprocess_get_exit_status(proc);
        g_debug("[chat] Claude: curl exit=%d, stderr=%zu bytes", exitStatus, stderrBuf.size());
        g_object_unref(proc);

        if (cancelled.load()) {
            return;
        }

        if (exitStatus != 0) {
            std::string msg = "Claude: curl exited with code " + std::to_string(exitStatus);
            if (!stderrBuf.empty()) {
                msg += ": " + stderrBuf;
            }
            onError(msg);
            return;
        }

        onComplete();
      } catch (const std::exception& e) {
        g_warning("[chat] Claude: unhandled exception: %s", e.what());
        try { onError(std::string("Claude: unexpected error — ") + e.what()); } catch (...) {}
      } catch (...) {
        g_warning("[chat] Claude: unknown unhandled exception");
        try { onError("Claude: unknown error occurred."); } catch (...) {}
      }
    }).detach();
}

void ClaudeModel::cancel() {
    cancelled.store(true);
}
