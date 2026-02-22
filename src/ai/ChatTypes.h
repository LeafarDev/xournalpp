/*
 * Xournal++
 *
 * Chat message types shared between AI backends.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <vector>

/**
 * Lightweight representation of a chat message.
 *
 * The role is intentionally a string ("system", "user", "assistant") to keep the
 * interface compatible with OpenAI-style HTTP APIs and external tools.
 */
struct Message {
    std::string role;
    std::string content;
};

using MessageList = std::vector<Message>;

