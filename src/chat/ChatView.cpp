/*
 * Xournal++
 *
 * Chat view: GTK + Pango + LaTeX (mini-PDF per message). Single code path for all platforms.
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ChatView.h"

#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "filesystem.h"

#include <gtk/gtk.h>
#include <poppler-document.h>
#include <poppler-page.h>
#include <cairo/cairo.h>

#include <glib.h>

#include "chat/ChatLatexCompiler.h"
#include "control/settings/LatexSettings.h"
#include "control/settings/Settings.h"
#include "latex/LatexParser.h"
#include "util/PathUtil.h"
#include "util/StringUtils.h"
#include "util/gtk4_helper.h"

struct ChatView::Impl {
    GtkWidget* outerBox = nullptr;       ///< VBox returned from getWidget()
    GtkScrolledWindow* scrolled = nullptr;
    GtkBox* messageBox = nullptr;
    // Download progress row (hidden unless a download is in progress)
    GtkWidget* progressRow = nullptr;    ///< HBox wrapping label + progressbar
    GtkWidget* progressLabel = nullptr;
    GtkWidget* progressBar = nullptr;
    std::string assistantBuffer;
    Settings* settings = nullptr;
    GtkWidget* thinkingRow = nullptr;
};

namespace {
constexpr double PDF_SCALE = 2.0;  // 144 DPI from 72

void pdf_draw_func(GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer user_data) {
    auto* doc = static_cast<PopplerDocument*>(user_data);
    if (!doc) return;
    PopplerPage* page = poppler_document_get_page(doc, 0);
    if (!page) return;
    double pw = 0, ph = 0;
    poppler_page_get_size(page, &pw, &ph);
    if (pw <= 0 || ph <= 0) {
        g_object_unref(page);
        return;
    }
    double sx = width / pw, sy = height / ph;
    cairo_scale(cr, sx, sy);
    poppler_page_render(page, cr);
    g_object_unref(page);
}

void pdf_doc_destroy_notify(gpointer data) {
    if (data) g_object_unref(static_cast<PopplerDocument*>(data));
}

GtkWidget* createPdfRenderWidget(const fs::path& pdfPath) {
    auto uri = Util::toUri(pdfPath);
    if (!uri || uri->empty()) return nullptr;
    GError* err = nullptr;
    PopplerDocument* doc = poppler_document_new_from_file(uri->c_str(), nullptr, &err);
    if (!doc) {
        if (err) g_error_free(err);
        return nullptr;
    }
    PopplerPage* page = poppler_document_get_page(doc, 0);
    if (!page) {
        g_object_unref(doc);
        return nullptr;
    }
    double pw = 0, ph = 0;
    poppler_page_get_size(page, &pw, &ph);
    g_object_unref(page);
    int w = std::max(1, static_cast<int>(std::ceil(pw * PDF_SCALE)));
    int h = std::max(1, static_cast<int>(std::ceil(ph * PDF_SCALE)));

    GtkWidget* da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, w, h);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da), pdf_draw_func, doc, pdf_doc_destroy_notify);
    return da;
}

struct LatexCompileResult {
    GtkWidget* placeholder = nullptr;
    bool ok = false;
    std::string pathOrError;
};

gboolean onLatexCompileDone(void* data) {
    auto* result = static_cast<LatexCompileResult*>(data);
    GtkWidget* placeholder = result->placeholder;
    GtkWidget* parent = placeholder ? gtk_widget_get_parent(placeholder) : nullptr;
    if (!parent) {
        delete result;
        return G_SOURCE_REMOVE;
    }
    GtkWidget* newChild = nullptr;
    if (result->ok) {
        fs::path p(result->pathOrError);
        newChild = createPdfRenderWidget(p);
    }
    if (!newChild) {
        const char* errText = result->ok ? "Failed to load PDF." :
                             (result->pathOrError.empty() ? "LaTeX compilation failed." : result->pathOrError.c_str());
        newChild = gtk_label_new(errText);
        gtk_label_set_wrap(GTK_LABEL(newChild), true);
        gtk_label_set_selectable(GTK_LABEL(newChild), true);
        gtk_widget_add_css_class(newChild, "chat-latex-error");
    }
    gtk_container_remove(GTK_CONTAINER(parent), placeholder);
    gtk_box_append(GTK_BOX(parent), newChild);
    gtk_widget_queue_resize(parent);
    delete result;
    return G_SOURCE_REMOVE;
}
}  // namespace

GtkWidget* ChatView::makeContentFromSegments(const std::string& content) {
    auto segments = xoj::latex::LatexParser::parse(content);
    g_debug("[chat] makeContentFromSegments: parsed %zu segments from %zu chars", segments.size(), content.size());
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(box, GTK_ALIGN_START);

    LatexSettings settingsCopy = impl->settings->latexSettings;

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];
        g_debug("[chat] segment %zu: type=%d, content=%zu chars", i, static_cast<int>(seg.type), seg.content.size());
        if (seg.type == xoj::latex::Segment::Type::TEXT) {
            std::string trimmed = StringUtils::trim(seg.content);
            if (!trimmed.empty()) {
                GtkWidget* label = gtk_label_new(seg.content.c_str());
                gtk_label_set_wrap(GTK_LABEL(label), true);
                gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
                gtk_label_set_selectable(GTK_LABEL(label), true);
                gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
                gtk_box_append(GTK_BOX(box), label);
            }
        } else {
            bool block = (seg.type == xoj::latex::Segment::Type::LATEX_BLOCK);
            if (seg.content.empty() || StringUtils::trim(seg.content).empty()) {
                g_debug("[chat] skipping empty LaTeX segment");
                continue;
            }
            g_debug("[chat] LaTeX segment: block=%d, content preview: %.100s", block, seg.content.c_str());
            GtkWidget* segmentBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            GtkWidget* placeholder = gtk_label_new("Renderizando LaTeX…");
            gtk_widget_add_css_class(placeholder, "chat-latex-loading");
            gtk_box_append(GTK_BOX(segmentBox), placeholder);
            gtk_box_append(GTK_BOX(box), segmentBox);

            auto* result = new LatexCompileResult{placeholder, false, {}};
            std::string latex = seg.content;
            std::thread([settingsCopy, latex, block, result]() {
                try {
                    g_debug("[chat] LaTeX compile thread: block=%d, content=%zu chars", block, latex.size());
                    auto r = xoj::chat::compileToPdf(settingsCopy, latex, block);
                    if (std::holds_alternative<fs::path>(r)) {
                        result->ok = true;
                        result->pathOrError = std::get<fs::path>(r).string();
                        g_debug("[chat] LaTeX compile success: %s", result->pathOrError.c_str());
                    } else {
                        result->pathOrError = std::get<std::string>(r);
                        g_debug("[chat] LaTeX compile error: %s", result->pathOrError.c_str());
                    }
                } catch (const std::exception& e) {
                    g_warning("[chat] LaTeX compile threw: %s", e.what());
                    result->ok = false;
                    result->pathOrError = std::string("LaTeX error: ") + e.what();
                } catch (...) {
                    g_warning("[chat] LaTeX compile threw unknown exception");
                    result->ok = false;
                    result->pathOrError = "Unknown LaTeX error.";
                }
                g_idle_add(onLatexCompileDone, result);
            }).detach();
        }
    }
    return box;
}

GtkWidget* ChatView::makeBubble(const std::string& cssClass, const std::string& content) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(row, "chat-bubble");
    gtk_widget_add_css_class(row, cssClass.c_str());

    GtkWidget* contentBox = makeContentFromSegments(content);
    gtk_widget_set_margin_start(contentBox, 10);
    gtk_widget_set_margin_end(contentBox, 10);
    gtk_widget_set_margin_top(contentBox, 8);
    gtk_widget_set_margin_bottom(contentBox, 8);
    gtk_widget_set_hexpand(contentBox, TRUE);
    gtk_box_append(GTK_BOX(row), contentBox);

    gtk_widget_set_halign(row, cssClass == "chat-user" ? GTK_ALIGN_END : GTK_ALIGN_START);
    return row;
}

ChatView::ChatView(Settings* settings): impl(std::make_unique<Impl>()) {
    impl->settings = settings;

    // Outer container: messages on top (expands), progress row on bottom (hidden).
    impl->outerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(impl->outerBox, true);
    gtk_widget_set_vexpand(impl->outerBox, true);

    impl->scrolled = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_policy(impl->scrolled, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(impl->scrolled, false);
    gtk_widget_set_hexpand(GTK_WIDGET(impl->scrolled), true);
    gtk_widget_set_vexpand(GTK_WIDGET(impl->scrolled), true);
    gtk_widget_set_size_request(GTK_WIDGET(impl->scrolled), 320, 200);
    gtk_widget_add_css_class(GTK_WIDGET(impl->scrolled), "chat-messages");

    impl->messageBox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));
    // Do NOT set vexpand on the message box — if it fills the scrolled window
    // there is nothing to scroll. Let its natural height drive the scroll range.
    gtk_widget_set_hexpand(GTK_WIDGET(impl->messageBox), TRUE);
    gtk_scrolled_window_set_child(impl->scrolled, GTK_WIDGET(impl->messageBox));

    gtk_box_append(GTK_BOX(impl->outerBox), GTK_WIDGET(impl->scrolled));

    // Progress row — hidden until a download starts.
    impl->progressRow = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(impl->progressRow, 8);
    gtk_widget_set_margin_end(impl->progressRow, 8);
    gtk_widget_set_margin_top(impl->progressRow, 4);
    gtk_widget_set_margin_bottom(impl->progressRow, 4);
    gtk_widget_add_css_class(impl->progressRow, "chat-download-progress");

    impl->progressLabel = gtk_label_new("Downloading...");
    gtk_label_set_xalign(GTK_LABEL(impl->progressLabel), 0.0f);
    gtk_widget_add_css_class(impl->progressLabel, "chat-download-label");
    gtk_box_append(GTK_BOX(impl->progressRow), impl->progressLabel);

    impl->progressBar = gtk_progress_bar_new();
    gtk_widget_set_hexpand(impl->progressBar, TRUE);
    gtk_box_append(GTK_BOX(impl->progressRow), impl->progressBar);

    gtk_box_append(GTK_BOX(impl->outerBox), impl->progressRow);
    gtk_widget_set_visible(impl->progressRow, FALSE);
}

ChatView::~ChatView() = default;

GtkWidget* ChatView::getWidget() {
    return impl->outerBox;
}

void ChatView::scrollToBottom() {
    // Defer to after GTK has computed the new layout so the adjustment
    // values (upper, page-size) reflect the newly added widgets.
    g_idle_add_full(
            G_PRIORITY_DEFAULT_IDLE,
            [](gpointer data) -> gboolean {
                auto* sw = GTK_SCROLLED_WINDOW(data);
                GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(sw);
                if (adj) {
                    double upper = gtk_adjustment_get_upper(adj);
                    double pageSize = gtk_adjustment_get_page_size(adj);
                    gtk_adjustment_set_value(adj, std::max(0.0, upper - pageSize));
                }
                return G_SOURCE_REMOVE;
            },
            g_object_ref(impl->scrolled),
            [](gpointer data) { g_object_unref(data); });
}

void ChatView::appendMessageRow(const std::string& cssClass, const std::string& content) {
    g_debug("[chat] appendMessageRow: %s, %zu chars", cssClass.c_str(), content.size());
    GtkWidget* row = makeBubble(cssClass, content);
    gtk_box_append(impl->messageBox, row);
    gtk_widget_set_visible(row, TRUE);
    gtk_widget_show_all(row);
    gtk_widget_queue_resize(GTK_WIDGET(impl->messageBox));
    gtk_widget_queue_draw(GTK_WIDGET(impl->scrolled));
    scrollToBottom();
}

void ChatView::addUserMessage(const std::string& text) {
    appendMessageRow("chat-user", text);
}

void ChatView::startAssistantMessage() {
    impl->assistantBuffer.clear();
    if (impl->thinkingRow) {
        GtkWidget* parent = gtk_widget_get_parent(impl->thinkingRow);
        if (parent) {
            gtk_container_remove(GTK_CONTAINER(parent), impl->thinkingRow);
        }
        impl->thinkingRow = nullptr;
    }
    GtkWidget* row = makeBubble("chat-assistant", "Thinking…");
    gtk_widget_add_css_class(row, "chat-thinking");
    gtk_box_append(impl->messageBox, row);
    gtk_widget_set_visible(row, TRUE);
    gtk_widget_show_all(row);
    impl->thinkingRow = row;
    gtk_widget_queue_resize(GTK_WIDGET(impl->messageBox));
    gtk_widget_queue_draw(GTK_WIDGET(impl->scrolled));
    scrollToBottom();
}

void ChatView::appendToken(const std::string& token) {
    impl->assistantBuffer += token;
}

void ChatView::finalizeMessage() {
    g_debug("[chat] finalizeMessage: buffer has %zu chars", impl->assistantBuffer.size());
    if (impl->assistantBuffer.size() > 0 && impl->assistantBuffer.size() < 200) {
        g_debug("[chat] finalizeMessage: buffer preview: %.200s", impl->assistantBuffer.c_str());
    }
    if (impl->thinkingRow && gtk_widget_get_parent(impl->thinkingRow)) {
        gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(impl->thinkingRow)), impl->thinkingRow);
        impl->thinkingRow = nullptr;
    }
    if (!impl->assistantBuffer.empty()) {
        appendMessageRow("chat-assistant", impl->assistantBuffer);
        impl->assistantBuffer.clear();
    }
    scrollToBottom();
}

void ChatView::clearThinkingPlaceholder() {
    if (impl->thinkingRow && gtk_widget_get_parent(impl->thinkingRow)) {
        gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(impl->thinkingRow)), impl->thinkingRow);
        impl->thinkingRow = nullptr;
        gtk_widget_queue_resize(GTK_WIDGET(impl->messageBox));
        scrollToBottom();
    }
}

void ChatView::showSystemNotice(const std::string& text) {
    appendMessageRow("chat-system", text);
}

void ChatView::clearConversation() {
    impl->thinkingRow = nullptr;
    GList* children = gtk_container_get_children(GTK_CONTAINER(impl->messageBox));
    for (GList* it = children; it; it = it->next) {
        gtk_container_remove(GTK_CONTAINER(impl->messageBox), GTK_WIDGET(it->data));
    }
    g_list_free(children);
    impl->assistantBuffer.clear();
}

void ChatView::showDownloadProgress(const std::string& label, double fraction) {
    gtk_label_set_text(GTK_LABEL(impl->progressLabel), label.c_str());
    if (fraction < 0.0) {
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(impl->progressBar));
    } else {
        double clamped = fraction > 1.0 ? 1.0 : fraction;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(impl->progressBar), clamped);
    }
    gtk_widget_set_visible(impl->progressRow, TRUE);
}

void ChatView::hideDownloadProgress() {
    gtk_widget_set_visible(impl->progressRow, FALSE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(impl->progressBar), 0.0);
}
