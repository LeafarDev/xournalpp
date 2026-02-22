/*
 * Xournal++
 *
 * High‑level chat controller: orchestrates context building, model routing,
 * and streaming of responses into a view.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ai/ChatTypes.h"
#include "ai/ContextBuilder.h"
#include "ai/DocumentContextProvider.h"
#include "ai/ModelRouter.h"

class Control;
class Document;
class MainWindow;
class Settings;
class IChatView;

class ChatController {
public:
    ChatController(Control* control, MainWindow* window, IChatView* view, Settings* settings);

    /// Send a user message through the active backend and stream the response
    /// into the view.
    void sendUserMessage(const std::string& text);

    /// Clear the in‑memory context and view.
    void clearConversation();

    /// Best‑effort cancellation of an in‑flight generation.
    void cancelCurrent();

    /// Set callback to be invoked when message completes (successfully or with error)
    void setCompletionCallback(std::function<void()> callback);

private:
    void sendUserMessageImpl(const std::string& text);

    Control* control;
    MainWindow* window;
    IChatView* view;
    Settings* settings;

    ContextBuilder context;
    std::unique_ptr<IChatModel> model;
    ModelRouter router;
    bool inFlight = false;
    std::function<void()> onCompletion;
};

