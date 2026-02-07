/*
 * Xournal++
 *
 * Context selector popover
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <gtk/gtk.h>

namespace xoj::chat {

struct ContextSelection {
    bool includeCurrentPage = true;
    bool includePageRange = false;
    bool includeSelectedText = true;
    bool includeWholeDocument = false;
    int rangeStart = 1;
    int rangeEnd = 1;
};

class ContextSelector {
public:
    ContextSelector();

    GtkWidget* getWidget() const;
    ContextSelection getSelection() const;
    void setRangeLimits(int minPage, int maxPage);

private:
    GtkWidget* popover = nullptr;
    GtkWidget* cbCurrent = nullptr;
    GtkWidget* cbRange = nullptr;
    GtkWidget* cbSelection = nullptr;
    GtkWidget* cbDocument = nullptr;
    GtkWidget* rangeStart = nullptr;
    GtkWidget* rangeEnd = nullptr;

    void updateSensitivity();
    static void onToggle(GtkToggleButton* button, gpointer userData);
};

}  // namespace xoj::chat
