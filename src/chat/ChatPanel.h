/*
 * Xournal++
 *
 * Chat side panel
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <gtk/gtk.h>

#include "chat/ChatInput.h"
#include "chat/ChatMessage.h"
#include "chat/ContextSelector.h"
#include "latex/LatexRenderer.h"
#include "filesystem.h"

class Control;
class MainWindow;

namespace xoj::chat {

class ChatPanel {
public:
    ChatPanel(Control* control, MainWindow* window);

    GtkWidget* getWidget() const;
    void focusInput();
    void clear();

private:
    Control* control;
    MainWindow* window;

    GtkWidget* root = nullptr;
    GtkWidget* listBox = nullptr;
    GtkWidget* scroller = nullptr;
    GtkWidget* header = nullptr;
    GtkWidget* contextButton = nullptr;
    GtkWidget* clearButton = nullptr;
    GtkWidget* closeButton = nullptr;
    GtkWidget* copilotLoginButton = nullptr;
    GtkWidget* modelCombo = nullptr;
    GtkWidget* contextCombo = nullptr;
    GtkWidget* contextSizeSpin = nullptr;
    GtkWidget* useGhCheck = nullptr;

    ContextSelector contextSelector;
    std::unique_ptr<class ChatInput> input;
    xoj::latex::LatexRenderer latexRenderer;

    std::atomic<bool> cancelRequested{false};

    void addMessage(Role role, const std::string& text);
    void addSystemMessage(const std::string& text);
    void sendMessage();
    void cancelGeneration();

    std::string buildContext(const std::string& contextId) const;
    std::string getSelectedModelId() const;
    std::string getSelectedContextId() const;
    void refreshModelChoices();
    void runModelOrCopilot(const std::string& modelPath, const std::string& question,
                           const std::string& context);
    void onCopilotLoginClicked();
    static std::string getCopilotPath();
};

}  // namespace xoj::chat
