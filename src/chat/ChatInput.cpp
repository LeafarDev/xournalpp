/*
 * Xournal++
 *
 * Chat input widget
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ChatInput.h"

#include "util/gtk4_helper.h"

namespace xoj::chat {

ChatInput::ChatInput() {
    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroller, -1, 80);

    textView = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textView), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(textView), false);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), textView);
    gtk_box_append(GTK_BOX(root), scroller);

    GtkWidget* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);

    cancelButton = gtk_button_new_with_label("Stop");
    sendButton = gtk_button_new_with_label("Send");
    gtk_box_append(GTK_BOX(actions), cancelButton);
    gtk_box_append(GTK_BOX(actions), sendButton);

    gtk_box_append(GTK_BOX(root), actions);

    g_signal_connect(textView, "key-press-event", G_CALLBACK(onKeyPress), this);
    g_signal_connect(sendButton, "clicked", G_CALLBACK(onSendClicked), this);
    g_signal_connect(cancelButton, "clicked", G_CALLBACK(onCancelClicked), this);
}

GtkWidget* ChatInput::getWidget() const { return root; }
GtkWidget* ChatInput::getTextView() const { return textView; }
GtkWidget* ChatInput::getSendButton() const { return sendButton; }
GtkWidget* ChatInput::getCancelButton() const { return cancelButton; }

void ChatInput::clear() {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView));
    gtk_text_buffer_set_text(buffer, "", -1);
}

std::string ChatInput::getText() const {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, false);
    std::string result = text ? text : "";
    g_free(text);
    return result;
}

void ChatInput::setEnabled(bool enabled) {
    gtk_widget_set_sensitive(textView, enabled);
    gtk_widget_set_sensitive(sendButton, enabled);
}

void ChatInput::focus() { gtk_widget_grab_focus(textView); }

void ChatInput::setSendCallback(std::function<void()> cb) { onSend = std::move(cb); }
void ChatInput::setCancelCallback(std::function<void()> cb) { onCancel = std::move(cb); }

void ChatInput::onSendClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<ChatInput*>(userData);
    if (self->onSend) {
        self->onSend();
    }
}

void ChatInput::onCancelClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<ChatInput*>(userData);
    if (self->onCancel) {
        self->onCancel();
    }
}

gboolean ChatInput::onKeyPress(GtkWidget*, GdkEventKey* event, gpointer userData) {
    auto* self = static_cast<ChatInput*>(userData);
    if (!self->onSend) {
        return FALSE;
    }
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        if ((event->state & GDK_SHIFT_MASK) != 0) {
            return FALSE;
        }
        self->onSend();
        return TRUE;
    }
    return FALSE;
}

}  // namespace xoj::chat
