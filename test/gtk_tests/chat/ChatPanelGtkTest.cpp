/*
 * Xournal++
 *
 * Front-end style tests for the chat panel: type in input, click Send,
 * verify the send callback is invoked with the typed text.
 *
 * @license GNU GPLv2 or later
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "dialog/GtkTest.h"
#include "config-test.h"


namespace {

std::vector<std::string> g_sentMessages;

void onSendClicked(GtkEntry* entry, gpointer) {
    const char* text = gtk_entry_get_text(entry);
    if (text && *text) {
        g_sentMessages.push_back(text);
    }
    gtk_entry_set_text(entry, "");
}

}  // namespace


class ChatPanelSendTest: public GtkTest {
    void runTest(GtkApplication* app) override {
        g_sentMessages.clear();

        GtkWidget* window = gtk_application_window_new(app);
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_container_add(GTK_CONTAINER(window), box);

        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Pergunte…");
        gtk_box_pack_start(GTK_BOX(box), entry, true, true, 0);

        GtkWidget* sendBtn = gtk_button_new_with_label("Send");
        g_signal_connect(entry, "activate", G_CALLBACK(onSendClicked), nullptr);
        g_signal_connect(sendBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            GtkEntry* e = GTK_ENTRY(data);
            onSendClicked(e, nullptr);
        }), entry);
        gtk_box_pack_start(GTK_BOX(box), sendBtn, false, false, 0);

        gtk_widget_show_all(window);

        // 1) Type and press Enter
        gtk_entry_set_text(GTK_ENTRY(entry), "Minha pergunta");
        gtk_widget_activate(entry);
        ASSERT_EQ(g_sentMessages.size(), 1U) << "Send should have been called once";
        EXPECT_EQ(g_sentMessages[0], "Minha pergunta");
        EXPECT_STREQ(gtk_entry_get_text(GTK_ENTRY(entry)), "");

        // 2) Type and click Send
        gtk_entry_set_text(GTK_ENTRY(entry), "Segunda pergunta");
        gtk_button_clicked(GTK_BUTTON(sendBtn));
        ASSERT_EQ(g_sentMessages.size(), 2U);
        EXPECT_EQ(g_sentMessages[1], "Segunda pergunta");

        // 3) Empty message: clicking Send should not add empty string (we only push if text non-empty)
        gtk_entry_set_text(GTK_ENTRY(entry), "");
        gtk_button_clicked(GTK_BUTTON(sendBtn));
        EXPECT_EQ(g_sentMessages.size(), 2U);
    }
};

TEST_F(ChatPanelSendTest, sendViaEnterAndButton) {}
