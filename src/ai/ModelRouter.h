/*
 * Xournal++
 *
 * Chat model router: chooses between local LLaMA, Copilot, etc.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>

#include "ai/IChatModel.h"

class Settings;

enum class ModelType {
    Local,
    Copilot,
    Claude,
    MPT7B,        ///< MPT-7B Q4_K_M, auto-downloaded from QuantFactory/mpt-7b-GGUF
    WizardMath7B, ///< WizardMath-7B-V1.1 Q4_K_M, math-specialised, auto-downloaded
    Llama32_1B    ///< Llama 3.2 1B Instruct Q4_K_M, auto-downloaded from bartowski/Llama-3.2-1B-Instruct-GGUF
};

class ModelRouter {
public:
    explicit ModelRouter(Settings* settings);

    std::unique_ptr<IChatModel> createModel(ModelType type) const;

    bool hasLocalModel() const;
    bool hasCopilot() const;

private:
    Settings* settings;
};

