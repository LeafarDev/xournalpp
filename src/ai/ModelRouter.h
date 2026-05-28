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
    MPT7B,                  ///< MPT-7B Q4_K_M
    WizardMath7B,           ///< WizardMath-7B-V1.1 Q4_K_M, math-specialised
    Llama32_1B,             ///< Llama 3.2 1B Instruct Q4_K_M
    Llama32_1B_Base,        ///< Llama 3.2 1B (base) Q4_K_M
    SmolLM2_135M_Instruct,  ///< SmolLM2 135M Instruct Q8_0
    SmolLM2_135M,           ///< SmolLM2 135M (base) Q8_0
    Qwen25_0_5B_Instruct,   ///< Qwen2.5 0.5B Instruct Q4_K_M
    Qwen25_0_5B,            ///< Qwen2.5 0.5B (base) Q4_K_M
    Qwen3_0_6B,             ///< Qwen3 0.6B Q4_K_M
    Qwen25Coder_0_5B,       ///< Qwen2.5-Coder 0.5B Instruct Q4_K_M
    TinyLlama_1_1B_Chat,    ///< TinyLlama 1.1B Chat Q4_K_M
    LFM25_1_2B_Instruct,    ///< LFM2.5 1.2B Instruct Q4_K_M (covers MLX 4/6/8-bit variants)
    LFM2_1_2B,              ///< LFM2 1.2B Q4_K_M
    Pythia_70M,             ///< Pythia 70M Q8_0
    Bloomz_560M,            ///< BLOOMZ 560M Q4_K_M
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

