/*
 * Xournal++
 *
 * High‑level chat controller.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/ChatController.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include <glib.h>

#include "ai/DownloadableLocalModel.h"
#include "chat/IChatView.h"
#include "core/control/Control.h"
#include "core/control/settings/Settings.h"
#include "core/gui/MainWindow.h"
#include "model/Document.h"
#include "util/PathUtil.h"
#include "util/Util.h"
#include "util/i18n.h"

ChatController::ChatController(Control* ctrl, MainWindow* win, IChatView* view, Settings* s):
        control(ctrl), window(win), view(view), settings(s), router(s) {}

void ChatController::sendUserMessage(const std::string& text) {
    // Guard the whole method so any unexpected exception is shown in the chat
    // rather than propagating into the GTK signal dispatcher.
    try {
    sendUserMessageImpl(text);
    } catch (const std::exception& e) {
        g_warning("[chat] sendUserMessage threw: %s", e.what());
        if (view) {
            view->hideDownloadProgress();
            view->clearThinkingPlaceholder();
            view->showSystemNotice(std::string("Error: ") + e.what());
        }
        inFlight = false;
        if (onCompletion) { onCompletion(); }
    } catch (...) {
        g_warning("[chat] sendUserMessage threw unknown exception");
        if (view) {
            view->hideDownloadProgress();
            view->clearThinkingPlaceholder();
            view->showSystemNotice("Unknown error sending message.");
        }
        inFlight = false;
        if (onCompletion) { onCompletion(); }
    }
}

void ChatController::sendUserMessageImpl(const std::string& text) {
    if (!control || !window || !view || !settings) {
        if (view) { view->showSystemNotice("Erro: Control, Window ou Settings não disponíveis."); }
        return;
    }
    if (text.empty()) {
        view->showSystemNotice("Por favor, digite uma mensagem.");
        return;
    }
    if (inFlight) {
        view->showSystemNotice("Aguarde a resposta anterior terminar antes de enviar outra mensagem.");
        return;
    }

    g_debug("[chat] sendUserMessage: displaying user message (%zu chars)", text.size());
    view->addUserMessage(text);
    context.addUserMessage(text);

    // --- Create model first so we know its context window size ---
    std::unique_ptr<IChatModel> newModel;
    std::string modelType = settings->getChatModel();
    try {
        if (modelType == "copilot") {
            newModel = router.createModel(ModelType::Copilot);
        } else if (modelType == "claude") {
            newModel = router.createModel(ModelType::Claude);
        } else if (modelType == "mpt-7b") {
            newModel = router.createModel(ModelType::MPT7B);
        } else if (modelType == "wizardmath-7b") {
            newModel = router.createModel(ModelType::WizardMath7B);
        } else if (modelType == "llama-3.2-1b") {
            newModel = router.createModel(ModelType::Llama32_1B);
        } else if (modelType == "llama-3.2-1b-base") {
            newModel = router.createModel(ModelType::Llama32_1B_Base);
        } else if (modelType == "smollm2-135m-instruct") {
            newModel = router.createModel(ModelType::SmolLM2_135M_Instruct);
        } else if (modelType == "smollm2-135m") {
            newModel = router.createModel(ModelType::SmolLM2_135M);
        } else if (modelType == "qwen2.5-0.5b-instruct") {
            newModel = router.createModel(ModelType::Qwen25_0_5B_Instruct);
        } else if (modelType == "qwen2.5-0.5b") {
            newModel = router.createModel(ModelType::Qwen25_0_5B);
        } else if (modelType == "qwen3-0.6b") {
            newModel = router.createModel(ModelType::Qwen3_0_6B);
        } else if (modelType == "qwen2.5-coder-0.5b-instruct") {
            newModel = router.createModel(ModelType::Qwen25Coder_0_5B);
        } else if (modelType == "tinyllama-1.1b-chat") {
            newModel = router.createModel(ModelType::TinyLlama_1_1B_Chat);
        } else if (modelType == "lfm2.5-1.2b-instruct") {
            newModel = router.createModel(ModelType::LFM25_1_2B_Instruct);
        } else if (modelType == "lfm2-1.2b") {
            newModel = router.createModel(ModelType::LFM2_1_2B);
        } else if (modelType == "pythia-70m") {
            newModel = router.createModel(ModelType::Pythia_70M);
        } else if (modelType == "bloomz-560m") {
            newModel = router.createModel(ModelType::Bloomz_560M);
        } else {
            newModel = router.createModel(ModelType::Local);
        }
    } catch (const std::exception& e) {
        std::string errorMsg = "Erro ao criar modelo: " + std::string(e.what());
        view->showSystemNotice(errorMsg);
        inFlight = false;
        if (onCompletion) {
            onCompletion();
        }
        return;
    } catch (...) {
        view->showSystemNotice("Erro desconhecido ao criar modelo.");
        inFlight = false;
        if (onCompletion) {
            onCompletion();
        }
        return;
    }

    if (!newModel) {
        std::string errorMsg = "Nenhum backend de chat disponível. ";
        if (modelType == "copilot") {
            errorMsg += "Verifique se o Copilot CLI está instalado e autenticado.";
        } else if (modelType == "claude") {
            errorMsg += "Enter your Anthropic API key in the 'API Key' field above the chat input.";
        } else if (modelType == "mpt-7b" || modelType == "wizardmath-7b" ||
                   modelType == "llama-3.2-1b" || modelType == "llama-3.2-1b-base" ||
                   modelType == "smollm2-135m-instruct" || modelType == "smollm2-135m" ||
                   modelType == "qwen2.5-0.5b-instruct" || modelType == "qwen2.5-0.5b" ||
                   modelType == "qwen3-0.6b" || modelType == "qwen2.5-coder-0.5b-instruct" ||
                   modelType == "tinyllama-1.1b-chat" || modelType == "lfm2.5-1.2b-instruct" ||
                   modelType == "lfm2-1.2b" || modelType == "pythia-70m" ||
                   modelType == "bloomz-560m") {
            errorMsg += "O modelo será baixado automaticamente ao enviar a primeira mensagem.";
        } else {
            errorMsg += "Configure XOURNALPP_LLM_MODEL com o caminho do modelo GGUF.";
        }
        view->showSystemNotice(errorMsg);
        inFlight = false;
        if (onCompletion) {
            onCompletion();
        }
        return;
    }

    model = std::move(newModel);

    // --- Build document context, capped to fit the model's context window ---
    // Reserve ~1500 tokens for system prompt header + latex-skill + user message + response buffer.
    // Use 3 chars/token as a conservative estimate for mixed LaTeX/text content.
    // Cloud models (maxContextTokens > 8000) effectively have no practical cap here.
    Document* doc = control->getDocument();
    DocumentContextProvider provider(doc, window);

    std::string ctxId = settings->getChatContext();
    DocContextMode mode = DocContextMode::CurrentPage;
    if (ctxId == "selection") {
        mode = DocContextMode::Selection;
    } else if (ctxId == "document") {
        mode = DocContextMode::FullDocument;
    } else if (ctxId == "none") {
        mode = DocContextMode::None;
    }

    int maxCtxChars = settings->getChatContextSize();
    if (maxCtxChars <= 0) {
        maxCtxChars = 12000;
    }
    {
        constexpr int kReservedTokens = 1500;
        constexpr int kCharsPerToken = 3;
        int modelCtx = static_cast<int>(model->params().maxContextTokens);
        int cap = std::max(0, modelCtx - kReservedTokens) * kCharsPerToken;
        if (cap > 0 && maxCtxChars > cap) {
            g_debug("[chat] capping document context from %d to %d chars (model ctx=%d tokens)",
                    maxCtxChars, cap, modelCtx);
            maxCtxChars = cap;
        }
    }
    std::string docContext = provider.buildContext(mode, maxCtxChars);

    // Build system prompt.
    std::string systemPrompt =
            "You are an AI assistant helping with math notes in Xournal++.\n"
            "CRITICAL: Your ENTIRE response must be written in LaTeX. Do NOT use Markdown, plain "
            "text tables, asterisks for bold/italic, or any non-LaTeX formatting.\n"
            "ALL text, lists, matrices, emphasis, and structure must use LaTeX syntax.\n"
            "LaTeX formatting rules:\n"
            "- Inline math: $...$ (e.g. $x^2$, $\\vec{v}$, $A_{ij}$)\n"
            "- Display math: $$...$$\n"
            "- Bold text: $\\textbf{word}$\n"
            "- Italic text: $\\textit{word}$\n"
            "- Matrices: $$\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}$$\n"
            "- Lists: \\begin{itemize}\\item ...\\end{itemize} or "
            "\\begin{enumerate}\\item ...\\end{enumerate}\n"
            "- Line break: \\\\\n"
            "- New paragraph: \\par or blank line\n"
            "NEVER output: | table | syntax |, **bold**, *italic*, or bare Markdown.\n"
            "ALWAYS use LaTeX equivalents for every formatting need.\n";

    // Append the LaTeX skill sheet only if there is room (local models have tight context limits).
    constexpr int kSkillSheetMaxChars = 6000;
    {
        int usedChars = static_cast<int>(systemPrompt.size()) + static_cast<int>(docContext.size());
        int remainingForSkill = model->params().maxContextTokens * 3 - usedChars;
        if (remainingForSkill > kSkillSheetMaxChars / 2) {
            fs::path skillPath = Util::getAiResourcePath("latex-skill.md");
            if (!skillPath.empty()) {
                std::ifstream f(skillPath);
                if (f.is_open()) {
                    std::ostringstream ss;
                    ss << f.rdbuf();
                    std::string skill = ss.str();
                    if (static_cast<int>(skill.size()) > kSkillSheetMaxChars) {
                        skill.resize(static_cast<size_t>(kSkillSheetMaxChars));
                    }
                    systemPrompt += "\n\n";
                    systemPrompt += skill;
                    g_debug("[chat] latex-skill.md appended (%zu bytes)", skill.size());
                }
            }
        } else {
            g_debug("[chat] skipping latex-skill.md (not enough context budget)");
        }
    }

    if (!docContext.empty()) {
        systemPrompt += "\n[Document Context]\n";
        systemPrompt += docContext;
        systemPrompt += "\n";
    }
    context.setSystemPrompt(std::move(systemPrompt));

    // Wire progress callback for downloadable models.
    if (auto* dlModel = dynamic_cast<DownloadableLocalModel*>(model.get())) {
        dlModel->setProgressCallback([this](double fraction, const std::string& label) {
            Util::execInUiThread([this, fraction, label]() {
                if (view) {
                    if (label.empty()) {
                        view->hideDownloadProgress();
                    } else {
                        view->showDownloadProgress(label, fraction);
                    }
                }
            });
        });
    }

    g_debug("[chat] model created, building history");

    auto history = context.buildForModel(model->params().maxContextTokens);
    g_debug("[chat] history built (%zu messages), starting assistant message", history.size());

    view->startAssistantMessage();
    inFlight = true;

    auto onToken = [this](const std::string& token) {
        g_debug("[chat] onToken called (%zu chars), scheduling UI update", token.size());
        Util::execInUiThread([this, token]() {
            try {
                g_debug("[chat] UI: appendToken running (%zu chars)", token.size());
                if (view) {
                    view->appendToken(token);
                }
            } catch (const std::exception& e) {
                g_warning("[chat] appendToken threw: %s", e.what());
            } catch (...) {
                g_warning("[chat] appendToken threw unknown exception");
            }
        });
    };

    auto onComplete = [this]() {
        g_debug("[chat] onComplete called (worker thread), scheduling UI finalize");
        Util::execInUiThread([this]() {
            try {
                g_debug("[chat] UI: finalizeMessage running");
                if (view) {
                    view->hideDownloadProgress();
                    view->finalizeMessage();
                }
            } catch (const std::exception& e) {
                g_warning("[chat] finalizeMessage threw: %s", e.what());
                if (view) { try { view->showSystemNotice(std::string("Render error: ") + e.what()); } catch (...) {} }
            } catch (...) {
                g_warning("[chat] finalizeMessage threw unknown exception");
            }
            inFlight = false;
            if (onCompletion) {
                onCompletion();
            }
        });
    };

    auto onError = [this](const std::string& err) {
        g_debug("[chat] onError called: %s", err.c_str());
        Util::execInUiThread([this, err]() {
            try {
                if (view) {
                    view->hideDownloadProgress();
                    view->clearThinkingPlaceholder();
                    view->showSystemNotice(err);
                }
            } catch (const std::exception& e) {
                g_warning("[chat] showSystemNotice threw: %s", e.what());
            } catch (...) {
                g_warning("[chat] showSystemNotice threw unknown exception");
            }
            inFlight = false;
            if (onCompletion) {
                onCompletion();
            }
        });
    };

    // Shared accumulator so both onToken and onComplete can access the full raw text.
    auto rawBuf = std::make_shared<std::string>();
    auto modelId = modelType;

    auto origOnToken = std::move(onToken);
    auto wrappedOnToken = [rawBuf, origOnToken](const std::string& token) mutable {
        *rawBuf += token;
        origOnToken(token);
    };

    auto origOnComplete = std::move(onComplete);
    auto wrappedOnComplete = [rawBuf, modelId, origOnComplete]() mutable {
        // Save raw response to debug file.
        try {
            namespace fs = std::filesystem;
            fs::path debugDir = fs::path(g_get_user_data_dir()) / "xournalpp";
            fs::create_directories(debugDir);
            // Use a timestamp so multiple responses are preserved.
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            std::string fname = "raw_response_" + std::to_string(ms) + "_" + modelId + ".txt";
            std::ofstream f(debugDir / fname);
            if (f.is_open()) {
                f << *rawBuf;
                g_debug("[chat] raw response saved to %s", (debugDir / fname).c_str());
            }
        } catch (...) {}
        origOnComplete();
    };

    g_debug("[chat] calling model->sendMessage");
    model->sendMessage(history, std::move(wrappedOnToken), std::move(wrappedOnComplete), std::move(onError));
}

void ChatController::clearConversation() {
    context.clear();
    if (view) {
        view->clearConversation();
    }
}

void ChatController::cancelCurrent() {
    if (model && inFlight) {
        model->cancel();
        if (view) {
            view->clearThinkingPlaceholder();
        }
        inFlight = false;
        if (onCompletion) {
            onCompletion();
        }
    }
}

void ChatController::setCompletionCallback(std::function<void()> callback) {
    onCompletion = std::move(callback);
}

