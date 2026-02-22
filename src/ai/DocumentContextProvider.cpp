/*
 * Xournal++
 *
 * Document context extraction for chat.
 *
 * @license GNU GPLv2 or later
 */

#include "ai/DocumentContextProvider.h"

#include <algorithm>

#include "ai/PDFContextExtractor.h"
#include "control/Control.h"
#include "core/gui/MainWindow.h"
#include "model/Document.h"
#include "util/Util.h"  // for npos

#include "control/tools/PdfElemSelection.h"
#include "gui/PdfFloatingToolbox.h"

DocumentContextProvider::DocumentContextProvider(Document* d, MainWindow* w): doc(d), window(w) {}

namespace {

std::string truncateContext(std::string text, int maxChars) {
    if (maxChars <= 0) {
        return text;
    }
    if (static_cast<int>(text.size()) <= maxChars) {
        return text;
    }
    text.resize(static_cast<std::size_t>(maxChars));
    text += "\n...";
    return text;
}

}  // namespace

std::string DocumentContextProvider::buildContext(DocContextMode mode, int maxChars) {
    if (!doc || !window) {
        return {};
    }

    if (mode == DocContextMode::None) {
        return {};
    }

    // Try to use selected text when requested.
    std::string selectedText;
    if (mode == DocContextMode::Selection) {
        if (auto* toolbox = window->getPdfToolbox(); toolbox && toolbox->hasSelection()) {
            if (auto* pdfSelection = toolbox->getSelection()) {
                selectedText = pdfSelection->getSelectedText();
            }
        }
        if (!selectedText.empty()) {
            return truncateContext(std::move(selectedText), maxChars);
        }
        // Fall through to current page if there is no selection.
        mode = DocContextMode::CurrentPage;
    }

    if (mode == DocContextMode::CurrentPage) {
        // Use the current page as context.
        // We need Control* to query the current page; MainWindow can give us Control.
        Control* control = window->getControl();
        if (!control) {
            return {};
        }
        std::size_t pageNo = control->getCurrentPageNo();
        std::string text = PDFContextExtractor::extract(doc, pageNo, "");
        return truncateContext(std::move(text), maxChars);
    }

    if (mode == DocContextMode::FullDocument) {
        // Concatenate text from all pages until we hit maxChars.
        std::string all;
        doc->lock();
        const std::size_t pageCount = doc->getPageCount();
        doc->unlock();

        for (std::size_t i = 0; i < pageCount; ++i) {
            if (!all.empty()) {
                all += "\n\n";
            }
            all += PDFContextExtractor::extract(doc, i, "");
            if (maxChars > 0 && static_cast<int>(all.size()) > maxChars) {
                all = truncateContext(std::move(all), maxChars);
                break;
            }
        }
        return all;
    }

    return {};
}

