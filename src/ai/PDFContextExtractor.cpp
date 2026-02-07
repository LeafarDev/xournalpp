#include "ai/PDFContextExtractor.h"

#include <algorithm>

#include "model/Document.h"
#include "model/PageRef.h"
#include "model/XojPage.h"
#include "pdf/base/XojPdfPage.h"
#include "util/Util.h"  // for npos

namespace {
constexpr size_t kMaxContextChars = 6000;

std::string truncateContext(std::string text) {
    if (text.size() <= kMaxContextChars) {
        return text;
    }
    text.resize(kMaxContextChars);
    text += "\n...";
    return text;
}

std::string extractPageText(Document* doc, size_t pageIndex) {
    if (!doc) {
        return {};
    }

    doc->lock();
    size_t pageCount = doc->getPageCount();
    if (pageIndex >= pageCount) {
        doc->unlock();
        return {};
    }

    PageRef page = doc->getPage(pageIndex);
    size_t pdfPageNr = page->getPdfPageNr();
    if (pdfPageNr == npos) {
        doc->unlock();
        return {};
    }

    XojPdfPageSPtr pdfPage = doc->getPdfPage(pdfPageNr);
    doc->unlock();

    if (!pdfPage) {
        return {};
    }

    XojPdfRectangle rect(0.0, 0.0, pdfPage->getWidth(), pdfPage->getHeight());
    return pdfPage->selectText(rect, XojPdfPageSelectionStyle::Area);
}
}

std::string PDFContextExtractor::extract(Document* doc, size_t currentPage, const std::string& selectedText) {
    if (!selectedText.empty()) {
        return truncateContext(selectedText);
    }

    if (doc == nullptr) {
        return {};
    }

    if (auto text = extractPageText(doc, currentPage); !text.empty()) {
        return truncateContext(text);
    }

    // Fallback: first page
    if (auto text = extractPageText(doc, 0); !text.empty()) {
        return truncateContext(text);
    }

    return {};
}
