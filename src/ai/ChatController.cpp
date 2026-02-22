/*
 * Xournal++
 *
 * High‑level chat controller.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/ChatController.h"

#include <utility>

#include <glib.h>

#include "ai/DownloadableLocalModel.h"
#include "chat/IChatView.h"
#include "core/control/Control.h"
#include "core/control/settings/Settings.h"
#include "core/gui/MainWindow.h"
#include "model/Document.h"
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

    Document* doc = control->getDocument();
    DocumentContextProvider provider(doc, window);

    // Map user preference string to DocContextMode.
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
    std::string docContext = provider.buildContext(mode, maxCtxChars);

    // Build system prompt (can be localized later).
    std::string systemPrompt =
            "You are an AI assistant helping with math notes in Xournal++.\n"
            "Use LaTeX delimiters so your output renders correctly:\n"
            "- Inline math/formatting: $...$ (e.g. $\\textbf{bold}$, $x^2$, $\\LaTeX$)\n"
            "- Display math: $$...$$\n"
            "- Lists: \\begin{itemize}...\\end{itemize} or \\begin{enumerate}...\\end{enumerate}\n"
            "- Line break: \\\\ or \\newline\n"
            "- New paragraph: blank line or \\par\n"
            "IMPORTANT: Wrap bold, italic, math and formatted text in $...$ so it renders. "
            "Use \\begin{...}...\\end{...} for lists.\n";
    if (!docContext.empty()) {
        systemPrompt += "\n[Document Context]\n";
        systemPrompt += docContext;
        systemPrompt += "\n";
    }
    context.setSystemPrompt(std::move(systemPrompt));

    // Decide backend from settings->getChatModel():
    //  - "copilot" -> Copilot
    //  - anything else -> local (via GGUF path lookup)
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
                   modelType == "llama-3.2-1b") {
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

    g_debug("[chat] calling model->sendMessage");
    model->sendMessage(history, std::move(onToken), std::move(onComplete), std::move(onError));
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

