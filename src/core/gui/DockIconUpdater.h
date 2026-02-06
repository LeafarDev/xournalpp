/*
 * Xournal++
 *
 * macOS: update Dock icon from PDF first page thumbnail.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

namespace xoj {

/**
 * macOS only: set Dock icon from the first page of a PDF (background thread,
 * then main thread). No-op on other platforms.
 */
void setDockIconFromPdfPath(const std::string& pdfPathUtf8);

/**
 * macOS only: clear Dock icon (restore default). No-op on other platforms.
 */
void clearDockIcon();

}  // namespace xoj

#ifndef __APPLE__
inline void xoj::setDockIconFromPdfPath(const std::string&) {}
inline void xoj::clearDockIcon() {}
#endif
