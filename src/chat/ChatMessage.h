/*
 * Xournal++
 *
 * Chat message rendering
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "latex/LatexParser.h"
#include "latex/LatexRenderer.h"

namespace xoj::chat {

enum class Role { USER, ASSISTANT, SYSTEM };

class ChatMessage {
public:
    ChatMessage(Role role, std::string text, xoj::latex::LatexRenderer* renderer = nullptr);

    GtkWidget* buildWidget();

private:
    Role role;
    std::string text;
    xoj::latex::LatexRenderer* renderer = nullptr;
};

}  // namespace xoj::chat
