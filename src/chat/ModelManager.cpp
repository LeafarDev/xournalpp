/*
 * Xournal++
 *
 * Chat model manager
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ModelManager.h"

#include "util/PathUtil.h"

namespace xoj::chat {

const std::vector<ModelInfo>& ModelManager::listModels() {
    static std::vector<ModelInfo> models = {
            {"copilot", "GitHub Copilot (gh)", "", "", 0ULL},
            {"mistral-7b-instruct", "Mistral 7B Instruct", "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
             "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/"
             "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
             4400000000ULL},
            {"phi3-mini-math", "Phi-3 Mini Math", "Phi-3-mini-4k-instruct-q4.gguf",
             "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-GGUF/resolve/main/"
             "Phi-3-mini-4k-instruct-q4.gguf",
             2282000000ULL},
            {"mathstral-7b", "Mathstral 7B", "mathstral-7B-v0.1-Q4_K_M.gguf",
             "https://huggingface.co/bartowski/mathstral-7B-v0.1-GGUF/resolve/main/mathstral-7B-v0.1-Q4_K_M.gguf",
             4370000000ULL},
            {"deepseek-math-7b", "DeepSeek Math 7B Base", "deepseek-math-7b-base-Q2_K.gguf",
             "https://huggingface.co/tensorblock/deepseek-math-7b-base-GGUF/resolve/main/"
             "deepseek-math-7b-base-Q2_K.gguf",
             2720000000ULL},
            {"mistral-pt-math", "Mistral Portuguese Math", "mistral-portuguese-luana-7b-mathematics.Q8_0.gguf",
             "https://huggingface.co/NikolayKozloff/Mistral-portuguese-luana-7b-Mathematics-Q8_0-GGUF/resolve/main/"
             "mistral-portuguese-luana-7b-mathematics.Q8_0.gguf",
             0ULL},
            {"mistral-math", "Mistral Instruct Math", "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
             "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/"
             "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
             4400000000ULL},
            {"qwen3-4b-math", "Qwen3 4B Math", "Qwen3-4B-Thinking-2507-Q4_K_M.gguf",
             "https://huggingface.co/unsloth/Qwen3-4B-Thinking-2507-GGUF/resolve/main/"
             "Qwen3-4B-Thinking-2507-Q4_K_M.gguf",
             2500000000ULL},
            {"phi3-mini", "Phi-3 Mini", "Phi-3-mini-4k-instruct-q4.gguf",
             "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-GGUF/resolve/main/"
             "Phi-3-mini-4k-instruct-q4.gguf",
             2282000000ULL},
    };
    return models;
}

std::optional<ModelInfo> ModelManager::findById(const std::string& id) {
    for (const auto& model: listModels()) {
        if (model.id == id) {
            return model;
        }
    }
    return std::nullopt;
}

fs::path ModelManager::modelsDir() { return Util::getDataSubfolder("models"); }

fs::path ModelManager::modelPath(const ModelInfo& model) { return modelsDir() / model.filename; }

bool ModelManager::isInstalled(const ModelInfo& model) {
    if (model.id == "copilot") {
        return true;  // virtual model, no file
    }
    return fs::exists(modelPath(model));
}

}  // namespace xoj::chat
