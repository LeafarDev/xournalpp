/*
 * Xournal++
 *
 * Chat model router: chooses between local LLaMA, Copilot, etc.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/ModelRouter.h"

#include <filesystem>

#include <glib.h>

#include "ai/ClaudeModel.h"
#include "ai/CopilotCliModel.h"
#include "ai/DownloadableLocalModel.h"
#include "ai/LocalLlamaModel.h"
#include "core/control/settings/Settings.h"
#include "util/PathUtil.h"

namespace fs = std::filesystem;

ModelRouter::ModelRouter(Settings* s): settings(s) {}

std::unique_ptr<IChatModel> ModelRouter::createModel(ModelType type) const {
    switch (type) {
    case ModelType::Local: {
        // For now, use XOURNALPP_LLM_MODEL to point to a local GGUF file.
        const char* envModel = g_getenv("XOURNALPP_LLM_MODEL");
        if (!envModel || !*envModel) {
            return nullptr;
        }
        fs::path modelPath = envModel;
        if (!fs::exists(modelPath) || !fs::is_regular_file(modelPath)) {
            return nullptr;
        }
        return std::make_unique<LocalLlamaModel>(modelPath.string());
    }
    case ModelType::Copilot: {
        return std::make_unique<CopilotCliModel>();
    }
    case ModelType::Claude: {
        return std::make_unique<ClaudeModel>(settings ? settings->getAnthropicApiKey() : std::string{});
    }
    case ModelType::MPT7B: {
        const LocalModelSpec* spec = DownloadableLocalModel::specForId("mpt-7b");
        if (!spec) {
            return nullptr;
        }
        return std::make_unique<DownloadableLocalModel>(*spec);
    }
    case ModelType::WizardMath7B: {
        const LocalModelSpec* spec = DownloadableLocalModel::specForId("wizardmath-7b");
        if (!spec) {
            return nullptr;
        }
        return std::make_unique<DownloadableLocalModel>(*spec);
    }
    case ModelType::Llama32_1B: {
        const LocalModelSpec* spec = DownloadableLocalModel::specForId("llama-3.2-1b");
        if (!spec) {
            return nullptr;
        }
        return std::make_unique<DownloadableLocalModel>(*spec);
    }
    }
    return nullptr;
}

bool ModelRouter::hasLocalModel() const {
    const char* envModel = g_getenv("XOURNALPP_LLM_MODEL");
    if (!envModel || !*envModel) {
        return false;
    }
    fs::path modelPath = envModel;
    return fs::exists(modelPath) && fs::is_regular_file(modelPath);
}

bool ModelRouter::hasCopilot() const {
    auto path = Util::getBundledCopilotPath();
    return !path.empty() && fs::exists(path) && fs::is_regular_file(path);
}


