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

/**
 * macOS only: set Dock badge label (e.g., filename). No-op on other platforms.
 */
void setDockBadgeLabel(const std::string& labelUtf8);

/**
 * macOS only: clear Dock badge label. No-op on other platforms.
 */
void clearDockBadgeLabel();

/**
 * macOS only: cancel any pending background dock-icon generation so the
 * process can exit cleanly. Call this early in the quit path.
 * No-op on other platforms.
 */
void cancelDockIconUpdate();

}  // namespace xoj

#ifndef __APPLE__
inline void xoj::setDockIconFromPdfPath(const std::string&) {}
inline void xoj::clearDockIcon() {}
inline void xoj::setDockBadgeLabel(const std::string&) {}
inline void xoj::clearDockBadgeLabel() {}
inline void xoj::cancelDockIconUpdate() {}
#endif
