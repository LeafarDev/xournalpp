#pragma once

#include <cstddef>
#include <string>

class Document;

class PDFContextExtractor {
public:
    static std::string extract(Document* doc, size_t currentPage, const std::string& selectedText);
};
