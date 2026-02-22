/*
 * Xournal++
 *
 * Chat view: GTK + Pango + LatexRenderer. Single implementation for all platforms.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>
#include <string>

#include <gtk/gtk.h>

#include "chat/IChatView.h"

class Settings;

class ChatView: public IChatView {
public:
    explicit ChatView(Settings* settings);
    ~ChatView() override;

    GtkWidget* getWidget();

    void addUserMessage(const std::string& text) override;
    void startAssistantMessage() override;
    void appendToken(const std::string& token) override;
    void finalizeMessage() override;
    void clearThinkingPlaceholder() override;
    void showSystemNotice(const std::string& text) override;
    void clearConversation() override;
    void showDownloadProgress(const std::string& label, double fraction) override;
    void hideDownloadProgress() override;

private:
    void appendMessageRow(const std::string& cssClass, const std::string& content);
    void scrollToBottom();
    GtkWidget* makeContentFromSegments(const std::string& content);
    GtkWidget* makeBubble(const std::string& cssClass, const std::string& content);

    struct Impl;
    std::unique_ptr<Impl> impl;
};
