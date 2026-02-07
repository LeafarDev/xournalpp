/*
 * Xournal++
 *
 * Chat input widget
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>
#include <string>

#include <gtk/gtk.h>

namespace xoj::chat {

class ChatInput {
public:
    ChatInput();

    GtkWidget* getWidget() const;
    GtkWidget* getTextView() const;
    GtkWidget* getSendButton() const;
    GtkWidget* getCancelButton() const;

    void clear();
    std::string getText() const;
    void setEnabled(bool enabled);
    void focus();

    void setSendCallback(std::function<void()> cb);
    void setCancelCallback(std::function<void()> cb);

private:
    GtkWidget* root = nullptr;
    GtkWidget* textView = nullptr;
    GtkWidget* sendButton = nullptr;
    GtkWidget* cancelButton = nullptr;

    std::function<void()> onSend;
    std::function<void()> onCancel;

    static gboolean onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer userData);
    static void onSendClicked(GtkButton* button, gpointer userData);
    static void onCancelClicked(GtkButton* button, gpointer userData);
};

}  // namespace xoj::chat
