/*
 * Xournal++
 *
 * Local GGUF model with auto-download from HuggingFace.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/DownloadableLocalModel.h"

#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#include <gio/gio.h>
#include <glib.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Built-in model catalogue
// ---------------------------------------------------------------------------

static const LocalModelSpec kBuiltinSpecs[] = {
        {
                "mpt-7b",
                "MPT-7B",
                "mpt-7b.Q4_K_M.gguf",
                "https://huggingface.co/QuantFactory/mpt-7b-GGUF/resolve/main/mpt-7b.Q4_K_M.gguf",
                4'586'496'000ULL,  // ~4.3 GB
        },
        {
                // MPT-7B-Math has no GGUF release; WizardMath-7B-V1.1 is the
                // best math-specialised 7B model available in GGUF format.
                "wizardmath-7b",
                "WizardMath-7B (Math)",
                "wizardmath-7b-v1.1.Q4_K_M.gguf",
                "https://huggingface.co/TheBloke/WizardMath-7B-V1.1-GGUF/resolve/main/"
                "wizardmath-7b-v1.1.Q4_K_M.gguf",
                4'369'375'296ULL,  // ~4.1 GB
        },
        {
                "llama-3.2-1b",
                "Llama 3.2 1B Instruct",
                "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/"
                "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
                807'403'520ULL,  // ~770 MB
        },
        {
                "llama-3.2-1b-base",
                "Llama 3.2 1B",
                "Llama-3.2-1B-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/Llama-3.2-1B-GGUF/resolve/main/"
                "Llama-3.2-1B-Q4_K_M.gguf",
                807'000'000ULL,  // ~770 MB
        },
        {
                "smollm2-135m-instruct",
                "SmolLM2 135M Instruct",
                "SmolLM2-135M-Instruct-Q8_0.gguf",
                "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/resolve/main/"
                "SmolLM2-135M-Instruct-Q8_0.gguf",
                152'000'000ULL,  // ~145 MB
        },
        {
                "smollm2-135m",
                "SmolLM2 135M",
                "SmolLM2-135M-Q8_0.gguf",
                "https://huggingface.co/bartowski/SmolLM2-135M-GGUF/resolve/main/"
                "SmolLM2-135M-Q8_0.gguf",
                152'000'000ULL,  // ~145 MB
        },
        {
                "qwen2.5-0.5b-instruct",
                "Qwen2.5 0.5B Instruct",
                "Qwen2.5-0.5B-Instruct-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/"
                "Qwen2.5-0.5B-Instruct-Q4_K_M.gguf",
                357'000'000ULL,  // ~340 MB
        },
        {
                "qwen2.5-0.5b",
                "Qwen2.5 0.5B",
                "Qwen2.5-0.5B-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/Qwen2.5-0.5B-GGUF/resolve/main/"
                "Qwen2.5-0.5B-Q4_K_M.gguf",
                357'000'000ULL,  // ~340 MB
        },
        {
                "qwen3-0.6b",
                "Qwen3 0.6B",
                "Qwen3-0.6B-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/Qwen3-0.6B-GGUF/resolve/main/"
                "Qwen3-0.6B-Q4_K_M.gguf",
                524'000'000ULL,  // ~500 MB
        },
        {
                "qwen2.5-coder-0.5b-instruct",
                "Qwen2.5-Coder 0.5B Instruct",
                "Qwen2.5-Coder-0.5B-Instruct-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/Qwen2.5-Coder-0.5B-Instruct-GGUF/resolve/main/"
                "Qwen2.5-Coder-0.5B-Instruct-Q4_K_M.gguf",
                378'000'000ULL,  // ~360 MB
        },
        {
                "tinyllama-1.1b-chat",
                "TinyLlama 1.1B Chat",
                "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
                "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/"
                "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
                669'000'000ULL,  // ~638 MB
        },
        {
                // LFM2.5-1.2B-Instruct covers all three MLX quantisation variants
                // (4-bit, 6-bit, 8-bit) listed in LM Studio; this is the GGUF Q4_K_M edition.
                "lfm2.5-1.2b-instruct",
                "LFM2.5 1.2B Instruct",
                "LFM2.5-1.2B-Instruct-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/LFM2.5-1.2B-Instruct-GGUF/resolve/main/"
                "LFM2.5-1.2B-Instruct-Q4_K_M.gguf",
                734'000'000ULL,  // ~700 MB
        },
        {
                "lfm2-1.2b",
                "LFM2 1.2B",
                "LFM2-1.2B-Q4_K_M.gguf",
                "https://huggingface.co/bartowski/LFM2-1.2B-GGUF/resolve/main/"
                "LFM2-1.2B-Q4_K_M.gguf",
                734'000'000ULL,  // ~700 MB
        },
        {
                "pythia-70m",
                "Pythia 70M",
                "pythia-70m-deduped.q8_0.gguf",
                "https://huggingface.co/afrideva/pythia-70m-deduped-GGUF/resolve/main/"
                "pythia-70m-deduped.q8_0.gguf",
                78'643'200ULL,  // ~75 MB
        },
        {
                "bloomz-560m",
                "BLOOMZ 560M",
                "bloomz-560m.Q4_K_M.gguf",
                "https://huggingface.co/mradermacher/bloomz-560m-GGUF/resolve/main/"
                "bloomz-560m.Q4_K_M.gguf",
                346'000'000ULL,  // ~330 MB
        },
};

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

fs::path DownloadableLocalModel::modelCacheDir() {
    const char* dataDir = g_get_user_data_dir();
    fs::path base = dataDir ? fs::path(dataDir) : (fs::path(g_get_home_dir()) / ".local/share");
    return base / "xournalpp" / "models";
}

bool DownloadableLocalModel::isModelCached(const std::string& filename) {
    return fs::exists(modelCacheDir() / filename);
}

const LocalModelSpec* DownloadableLocalModel::specForId(const std::string& id) {
    for (const auto& s: kBuiltinSpecs) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// DownloadableLocalModel
// ---------------------------------------------------------------------------

DownloadableLocalModel::DownloadableLocalModel(LocalModelSpec s): spec(std::move(s)) {
    p.id = spec.id;
    p.displayName = spec.displayName;
    p.maxContextTokens = 4096;
}

void DownloadableLocalModel::setProgressCallback(
        std::function<void(double, const std::string&)> cb) {
    progressCallback = std::move(cb);
}

// Download the model via curl, streaming progress tokens.
// Returns true on success.
bool DownloadableLocalModel::downloadModel(const std::function<void(const std::string&)>& /*onToken*/,
                                           const std::function<void(const std::string&)>& onError) {
    std::error_code ec;
    fs::create_directories(modelCacheDir(), ec);
    if (ec) {
        onError("Could not create model cache directory: " + ec.message());
        return false;
    }

    auto dest = modelCacheDir() / spec.filename;
    auto tmp = modelCacheDir() / (spec.filename + ".part");

    // Human-readable size hint.
    std::string sizeHint;
    if (spec.sizeBytes > 0) {
        double gb = static_cast<double>(spec.sizeBytes) / (1024.0 * 1024.0 * 1024.0);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f GB", gb);
        sizeHint = std::string(" (~") + buf + ")";
    }

    // Notify via progress callback (indeterminate while starting).
    std::string progressLabel = "Downloading " + spec.displayName + sizeHint;
    if (progressCallback) {
        progressCallback(-1.0, progressLabel);
    }

    // curl flags:
    //   -L   follow redirects (HuggingFace uses them)
    //   -#   hash-bar progress to stderr instead of the verbose table
    //   -C - resume a partial download if .part exists
    //   -o   output file
    GError* gerr = nullptr;
    GSubprocess* proc =
            g_subprocess_new(static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDERR_PIPE), &gerr,
                             "curl", "-L", "-#", "-C", "-", "-o", tmp.string().c_str(),
                             spec.url.c_str(), nullptr);

    if (!proc) {
        std::string msg = gerr ? gerr->message : "curl not found";
        if (gerr) {
            g_error_free(gerr);
        }
        onError("Download failed (cannot launch curl): " + msg);
        return false;
    }

    // Parse curl -# progress from stderr.
    // curl -# writes lines like:  "########          20.0%"  terminated with \r.
    GInputStream* errStream = g_subprocess_get_stderr_pipe(proc);
    std::string pending;
    std::string lastPct;
    char buf[512];

    while (!cancelled.load()) {
        GError* re = nullptr;
        gssize n = g_input_stream_read(errStream, buf, sizeof(buf) - 1, nullptr, &re);
        if (n <= 0) {
            if (re) {
                g_error_free(re);
            }
            break;
        }
        buf[n] = '\0';
        pending.append(buf, static_cast<size_t>(n));

        // Split on CR or LF.
        size_t pos;
        while ((pos = pending.find_first_of("\r\n")) != std::string::npos) {
            std::string seg = pending.substr(0, pos);
            pending.erase(0, pos + 1);

            // Look for "42.0%" at the end of the segment.
            auto ppos = seg.rfind('%');
            if (ppos != std::string::npos && ppos > 0) {
                size_t start = ppos;
                while (start > 0 &&
                       (seg[start - 1] == '.' ||
                        std::isdigit(static_cast<unsigned char>(seg[start - 1])))) {
                    --start;
                }
                if (start < ppos) {
                    // pct is the numeric part without the '%' sign
                    std::string pct = seg.substr(start, ppos - start);
                    if (pct != lastPct) {
                        lastPct = pct;
                        if (progressCallback) {
                            try {
                                double fraction = std::stod(pct) / 100.0;
                                progressCallback(fraction, progressLabel);
                            } catch (...) {}
                        }
                    }
                }
            }
        }
    }

    g_subprocess_wait(proc, nullptr, nullptr);
    gint exitCode = g_subprocess_get_exit_status(proc);
    g_object_unref(proc);

    if (cancelled.load()) {
        fs::remove(tmp, ec);
        return false;
    }

    if (exitCode != 0) {
        fs::remove(tmp, ec);
        onError("Download failed (curl exited with code " + std::to_string(exitCode) +
                "). Check your internet connection and try again.");
        return false;
    }

    // Atomically commit the downloaded file.
    fs::rename(tmp, dest, ec);
    if (ec) {
        onError("Could not finalize download: " + ec.message());
        return false;
    }

    if (progressCallback) {
        progressCallback(1.0, progressLabel);
    }
    return true;
}

void DownloadableLocalModel::sendMessage(const MessageList& history,
                                         std::function<void(const std::string&)> onToken,
                                         std::function<void()> onComplete,
                                         std::function<void(const std::string&)> onError) {
    if (!onToken || !onComplete || !onError) {
        return;
    }

    cancelled.store(false);

    std::thread([this, history, onToken, onComplete, onError]() mutable {
      try {
        // Ensure model is downloaded and engine is ready.
        if (!engineReady) {
            auto modelPath = modelCacheDir() / spec.filename;

            if (!fs::exists(modelPath)) {
                g_debug("[chat] %s not found, starting download", spec.filename.c_str());
                if (!downloadModel(onToken, onError)) {
                    return;
                }
                if (cancelled.load()) {
                    return;
                }
            }

            if (progressCallback) {
                progressCallback(-1.0, "Initializing " + spec.displayName + "...");
            }
            engineReady = engine.init(modelPath.string());
            if (!engineReady) {
                if (progressCallback) {
                    progressCallback(0.0, "");
                }
                onError("Failed to load " + spec.displayName +
                        ". The GGUF file may be incomplete or incompatible with this build of "
                        "llama.cpp.");
                return;
            }
            // Sync actual context window so ChatController can cap document context correctly.
            if (int ctxSize = engine.getContextSize(); ctxSize > 0) {
                p.maxContextTokens = static_cast<std::size_t>(ctxSize);
            }
            if (progressCallback) {
                progressCallback(1.0, "Model ready");
            }
        }

        // Build prompt.
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

        std::string output = engine.run(oss.str());

        if (cancelled.load()) {
            return;
        }

        if (output.empty()) {
            onError("Model returned empty output. Try a shorter prompt or restart Xournal++.");
            return;
        }

        g_debug("[chat] %s: generated %zu chars", spec.id.c_str(), output.size());
        onToken(output);
        onComplete();
      } catch (const std::exception& e) {
        g_warning("[chat] %s: unhandled exception: %s", spec.id.c_str(), e.what());
        if (progressCallback) { try { progressCallback(0.0, ""); } catch (...) {} }
        try { onError(spec.displayName + ": unexpected error — " + e.what()); } catch (...) {}
      } catch (...) {
        g_warning("[chat] %s: unknown unhandled exception", spec.id.c_str());
        if (progressCallback) { try { progressCallback(0.0, ""); } catch (...) {} }
        try { onError(spec.displayName + ": unknown error occurred."); } catch (...) {}
      }
    }).detach();
}

void DownloadableLocalModel::cancel() {
    cancelled.store(true);
}
