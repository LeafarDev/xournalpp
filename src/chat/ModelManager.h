/*
 * Xournal++
 *
 * Chat model manager
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "filesystem.h"

namespace xoj::chat {

struct ModelInfo {
    std::string id;
    std::string name;
    std::string filename;
    std::string url;
    size_t sizeBytes = 0;
};

class ModelManager {
public:
    static const std::vector<ModelInfo>& listModels();
    static std::optional<ModelInfo> findById(const std::string& id);
    static fs::path modelsDir();
    static fs::path modelPath(const ModelInfo& model);
    static bool isInstalled(const ModelInfo& model);
};

}  // namespace xoj::chat
