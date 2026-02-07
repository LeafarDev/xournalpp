/*
 * Xournal++
 *
 * Context selector popover
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ContextSelector.h"

#include "util/gtk4_helper.h"

namespace xoj::chat {

ContextSelector::ContextSelector() {
    popover = gtk_popover_new();

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_popover_set_child(GTK_POPOVER(popover), box);

    cbCurrent = gtk_check_button_new_with_label("Current page");
    cbRange = gtk_check_button_new_with_label("Page range");
    cbSelection = gtk_check_button_new_with_label("Selected text");
    cbDocument = gtk_check_button_new_with_label("Whole document (may be slow)");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbCurrent), true);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbSelection), true);

    gtk_box_append(GTK_BOX(box), cbCurrent);
    gtk_box_append(GTK_BOX(box), cbRange);

    GtkWidget* rangeBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* rangeLabel = gtk_label_new("From");
    rangeStart = gtk_spin_button_new_with_range(1, 1, 1);
    GtkWidget* rangeLabel2 = gtk_label_new("to");
    rangeEnd = gtk_spin_button_new_with_range(1, 1, 1);
    gtk_box_append(GTK_BOX(rangeBox), rangeLabel);
    gtk_box_append(GTK_BOX(rangeBox), rangeStart);
    gtk_box_append(GTK_BOX(rangeBox), rangeLabel2);
    gtk_box_append(GTK_BOX(rangeBox), rangeEnd);
    gtk_box_append(GTK_BOX(box), rangeBox);

    gtk_box_append(GTK_BOX(box), cbSelection);
    gtk_box_append(GTK_BOX(box), cbDocument);

    g_signal_connect(cbCurrent, "toggled", G_CALLBACK(onToggle), this);
    g_signal_connect(cbRange, "toggled", G_CALLBACK(onToggle), this);
    g_signal_connect(cbSelection, "toggled", G_CALLBACK(onToggle), this);
    g_signal_connect(cbDocument, "toggled", G_CALLBACK(onToggle), this);

    updateSensitivity();
}

GtkWidget* ContextSelector::getWidget() const { return popover; }

ContextSelection ContextSelector::getSelection() const {
    ContextSelection selection;
    selection.includeCurrentPage = gtk_check_button_get_active(GTK_CHECK_BUTTON(cbCurrent));
    selection.includePageRange = gtk_check_button_get_active(GTK_CHECK_BUTTON(cbRange));
    selection.includeSelectedText = gtk_check_button_get_active(GTK_CHECK_BUTTON(cbSelection));
    selection.includeWholeDocument = gtk_check_button_get_active(GTK_CHECK_BUTTON(cbDocument));
    selection.rangeStart = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rangeStart));
    selection.rangeEnd = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rangeEnd));
    return selection;
}

void ContextSelector::setRangeLimits(int minPage, int maxPage) {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rangeStart), minPage, maxPage);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rangeEnd), minPage, maxPage);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(rangeStart), minPage);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(rangeEnd), maxPage);
}

void ContextSelector::updateSensitivity() {
    bool enableRange = gtk_check_button_get_active(GTK_CHECK_BUTTON(cbRange));
    gtk_widget_set_sensitive(rangeStart, enableRange);
    gtk_widget_set_sensitive(rangeEnd, enableRange);
}

void ContextSelector::onToggle(GtkToggleButton*, gpointer userData) {
    auto* self = static_cast<ContextSelector*>(userData);
    if (self) {
        self->updateSensitivity();
    }
}

}  // namespace xoj::chat
