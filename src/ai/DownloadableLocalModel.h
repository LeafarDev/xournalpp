/*
 * Xournal++
 *
 * Local GGUF model that auto-downloads from HuggingFace if not cached.
 *
 * Supported built-in specs:
 *   "mpt-7b"        – MPT-7B (base) Q4_K_M from QuantFactory/mpt-7b-GGUF
 *   "wizardmath-7b" – WizardMath-7B V1.1 Q4_K_M from TheBloke
 *                     (used as the math-focused variant; MPT-7B-Math has
 *                      no GGUF release as of 2024)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

#include "ai/IChatModel.h"
#include "ai/LLMEngine.h"

struct LocalModelSpec {
    std::string id;           ///< e.g. "mpt-7b"
    std::string displayName;  ///< e.g. "MPT-7B"
    std::string filename;     ///< GGUF filename on disk and in HuggingFace repo
    std::string url;          ///< Full direct-download URL
    size_t sizeBytes = 0;     ///< Approximate size for user display
};

class DownloadableLocalModel: public IChatModel {
public:
    explicit DownloadableLocalModel(LocalModelSpec spec);
    ~DownloadableLocalModel() override = default;

    const Params& params() const override { return p; }

    void sendMessage(const MessageList& history,
                     std::function<void(const std::string&)> onToken,
                     std::function<void()> onComplete,
                     std::function<void(const std::string&)> onError) override;

    void cancel() override;

    /// Directory where downloaded GGUF files are cached.
    static std::filesystem::path modelCacheDir();

    /// True if the model file already exists in the cache directory.
    static bool isModelCached(const std::string& filename);

    /// Built-in spec for the given model id, or nullptr if unknown.
    static const LocalModelSpec* specForId(const std::string& id);

    /// Set an optional progress callback invoked during download.
    /// Called from a worker thread; the callee must marshal to the UI thread.
    /// @param cb  (fraction ∈ [0,1], label string) or fraction < 0 for indeterminate.
    void setProgressCallback(std::function<void(double, const std::string&)> cb);

private:
    LocalModelSpec spec;
    Params p;
    LLMEngine engine;
    bool engineReady = false;
    std::atomic<bool> cancelled{false};

    bool downloadModel(const std::function<void(const std::string&)>& onToken,
                       const std::function<void(const std::string&)>& onError);

    std::function<void(double, const std::string&)> progressCallback;
};
