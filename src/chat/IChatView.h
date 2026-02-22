/*
 * Xournal++
 *
 * Abstract chat view interface.
 *
 * Implementations are responsible for rendering assistant messages and system
 * notices (e.g. GTK listbox, macOS WebView). No AI logic is allowed in views;
 * they simply display content pushed from C++.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

class IChatView {
public:
    virtual ~IChatView() = default;

    /// Add a user message to the conversation view.
    virtual void addUserMessage(const std::string& text) = 0;

    /// Begin a new assistant message. Subsequent appendToken() calls will add
    /// to this message until finalizeMessage() is called.
    virtual void startAssistantMessage() = 0;

    /// Append a chunk of text to the current assistant message.
    virtual void appendToken(const std::string& token) = 0;

    /// Finish the current assistant message.
    virtual void finalizeMessage() = 0;

    /// Remove the "Thinking..." placeholder (e.g. on error or cancel).
    virtual void clearThinkingPlaceholder() = 0;

    /// Show a one‑off system notice (errors, offline status, etc.).
    virtual void showSystemNotice(const std::string& text) = 0;

    /// Clear all rendered conversation messages.
    virtual void clearConversation() = 0;

    /// Show (or update) the download progress bar.
    /// @param label    Short description, e.g. "Downloading MPT-7B (~4.3 GB)"
    /// @param fraction Progress in [0, 1], or < 0 for indeterminate pulse.
    virtual void showDownloadProgress(const std::string& label, double fraction) = 0;

    /// Hide the download progress bar.
    virtual void hideDownloadProgress() = 0;
};

