/*
 * Xournal++
 *
 * Document context extraction for chat.
 *
 * Builds textual context (selection, current page, full document) to be injected
 * into the system prompt for AI models.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

class Document;
class MainWindow;

enum class DocContextMode {
    None,
    CurrentPage,
    Selection,
    FullDocument
};

class DocumentContextProvider {
public:
    DocumentContextProvider(Document* doc, MainWindow* window);

    /**
     * Build a textual context string according to @p mode, truncated to at most
     * @p maxChars characters (adding "..." when truncated).
     */
    std::string buildContext(DocContextMode mode, int maxChars);

private:
    Document* doc;
    MainWindow* window;
};

