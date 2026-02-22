/*
 * Xournal++
 *
 * Unit tests for ChatController (validation and error reporting).
 *
 * @license GNU GPLv2 or later
 */

#include <gtest/gtest.h>

#include "ai/ChatController.h"
#include "chat/IChatView.h"


namespace {

class MockChatView: public IChatView {
public:
    void addUserMessage(const std::string& text) override {
        userMessages.push_back(text);
    }
    void startAssistantMessage() override { ++startAssistantCount; }
    void appendToken(const std::string& token) override { lastAppendedToken = token; }
    void finalizeMessage() override { ++finalizeCount; }
    void showSystemNotice(const std::string& text) override {
        systemNotices.push_back(text);
    }
    void clearConversation() override { ++clearCount; }

    std::vector<std::string> userMessages;
    std::vector<std::string> systemNotices;
    std::string lastAppendedToken;
    int startAssistantCount = 0;
    int finalizeCount = 0;
    int clearCount = 0;
};

}  // namespace


TEST(ChatController, nullControlShowsErrorNotice) {
    MockChatView view;
    // Control=null, Window=null, View=&view, Settings=null -> should show error and not crash
    ChatController ctrl(nullptr, nullptr, &view, nullptr);
    ctrl.sendUserMessage("hello");
    ASSERT_FALSE(view.systemNotices.empty());
    EXPECT_NE(view.systemNotices[0].find("Control"), std::string::npos);
    EXPECT_TRUE(view.userMessages.empty());
}

TEST(ChatController, clearConversationCallsView) {
    MockChatView view;
    ChatController ctrl(nullptr, nullptr, &view, nullptr);
    ctrl.clearConversation();
    EXPECT_EQ(view.clearCount, 1);
}

TEST(ChatController, cancelCurrentNoCrash) {
    MockChatView view;
    ChatController ctrl(nullptr, nullptr, &view, nullptr);
    ctrl.cancelCurrent();  // No-op when no model; must not crash
}
