/*
 * Xournal++
 *
 * Unit tests for ContextBuilder (token-aware chat context).
 *
 * @license GNU GPLv2 or later
 */

#include <gtest/gtest.h>

#include "ai/ContextBuilder.h"
#include "ai/ChatTypes.h"


TEST(ContextBuilder, empty) {
    ContextBuilder b;
    auto out = b.buildForModel(1000);
    EXPECT_TRUE(out.empty());
}

TEST(ContextBuilder, systemPromptOnly) {
    ContextBuilder b;
    b.setSystemPrompt("You are helpful.");
    auto out = b.buildForModel(1000);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0].role, "system");
    EXPECT_EQ(out[0].content, "You are helpful.");
}

TEST(ContextBuilder, addUserAndAssistant) {
    ContextBuilder b;
    b.setSystemPrompt("Sys");
    b.addUserMessage("Hello");
    b.addAssistantMessage("Hi there");
    b.addUserMessage("Bye");
    auto out = b.buildForModel(2000);
    ASSERT_GE(out.size(), 3U);
    EXPECT_EQ(out[0].role, "system");
    EXPECT_EQ(out[1].role, "user");
    EXPECT_EQ(out[1].content, "Hello");
    EXPECT_EQ(out[2].role, "assistant");
    EXPECT_EQ(out[2].content, "Hi there");
    EXPECT_EQ(out[3].role, "user");
    EXPECT_EQ(out[3].content, "Bye");
}

TEST(ContextBuilder, clear) {
    ContextBuilder b;
    b.setSystemPrompt("Sys");
    b.addUserMessage("Hi");
    b.clear();
    auto out = b.buildForModel(1000);
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(b.rawHistory().empty());
}

TEST(ContextBuilder, tokenLimitDropsOldest) {
    ContextBuilder b;
    b.setSystemPrompt("S");  // ~1 token
    // ~25 tokens each (100 chars / 4)
    b.addUserMessage(std::string(100, 'a'));
    b.addAssistantMessage(std::string(100, 'b'));
    b.addUserMessage(std::string(100, 'c'));
    b.addAssistantMessage(std::string(100, 'd'));
    b.addUserMessage(std::string(100, 'e'));
    auto out = b.buildForModel(40);  // system + a few messages
    // Should have system + newest messages only (e, d, c, ...)
    EXPECT_EQ(out[0].role, "system");
    std::size_t historyCount = out.size() - 1;
    EXPECT_GT(historyCount, 0U);
    EXPECT_LT(historyCount, 5U);
    // Newest should be last
    EXPECT_EQ(out.back().content, std::string(100, 'e'));
}

TEST(ContextBuilder, zeroMaxTokensReturnsAll) {
    ContextBuilder b;
    b.setSystemPrompt("System");
    b.addUserMessage("U1");
    b.addAssistantMessage("A1");
    auto out = b.buildForModel(0);
    ASSERT_EQ(out.size(), 3U);
    EXPECT_EQ(out[0].role, "system");
    EXPECT_EQ(out[1].role, "user");
    EXPECT_EQ(out[2].role, "assistant");
}

TEST(ContextBuilder, rawHistoryOrder) {
    ContextBuilder b;
    b.addUserMessage("first");
    b.addAssistantMessage("second");
    b.addUserMessage("third");
    const auto& h = b.rawHistory();
    ASSERT_EQ(h.size(), 3U);
    EXPECT_EQ(h[0].content, "first");
    EXPECT_EQ(h[1].content, "second");
    EXPECT_EQ(h[2].content, "third");
}
