/*
 * Xournal++
 *
 * Chat message rendering
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ChatMessage.h"

#include "latex/LatexRenderer.h"
#include "util/gtk4_helper.h"

#include <glib.h>

#include <cstring>
#include <utility>

namespace xoj::chat {
namespace {

/// Replace common LaTeX commands in plain text with Unicode (so "1 \times 3" shows as "1 × 3")
void replaceCommonLatexWithUnicode(std::string& s) {
    auto replace = [&s](const char* from, const char* to) {
        size_t len = strlen(from);
        for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += strlen(to))
            s.replace(pos, len, to);
    };
    replace("\\times", "×");
    replace("\\div", "÷");
    replace("\\pm", "±");
    replace("\\mp", "∓");
    replace("\\leq", "≤");
    replace("\\geq", "≥");
    replace("\\neq", "≠");
    replace("\\approx", "≈");
    replace("\\infty", "∞");
    replace("\\cdot", "·");
    replace("\\ldots", "…");
    replace("\\sqrt", "√");
    replace("\\alpha", "α");
    replace("\\beta", "β");
    replace("\\gamma", "γ");
    replace("\\delta", "δ");
    replace("\\theta", "θ");
    replace("\\pi", "π");
    replace("\\sum", "Σ");
    replace("\\prod", "∏");
    replace("\\int", "∫");
    replace("\\rightarrow", "→");
    replace("\\leftarrow", "←");
    replace("\\Rightarrow", "⇒");
    replace("\\Leftarrow", "⇐");
    replace("\\quad", " ");
    replace("\\qquad", "  ");
    replace("\\,", " ");
    replace("\\;", " ");
    replace("\\!", "");
    // \frac{a}{b} -> a/b (simple: find \frac{ then }{ then }
    size_t pos = 0;
    while ((pos = s.find("\\frac{", pos)) != std::string::npos) {
        size_t start = pos;
        pos += 6;
        size_t brace1 = s.find('}', pos);
        if (brace1 == std::string::npos) break;
        size_t mid = s.find("}{", brace1);
        if (mid == std::string::npos) break;
        size_t brace2 = s.find('}', mid + 2);
        if (brace2 == std::string::npos) break;
        std::string num = s.substr(pos, brace1 - pos);
        std::string den = s.substr(mid + 2, brace2 - (mid + 2));
        std::string repl = "(" + num + ")/(" + den + ")";
        s.replace(start, brace2 - start + 1, repl);
        pos = start + repl.size();
    }
    // \begin{bmatrix} ... \end{bmatrix} and similar: rows separated by \\ -> " ; "
    struct EnvPair { const char* begin; size_t beginLen; const char* end; };
    for (const EnvPair& p : {
             EnvPair{"\\begin{bmatrix}", 16, "\\end{bmatrix}"},
             EnvPair{"\\begin{pmatrix}", 16, "\\end{pmatrix}"},
             EnvPair{"\\begin{matrix}", 15, "\\end{matrix}"},
             EnvPair{"\\begin{Bmatrix}", 16, "\\end{Bmatrix}"}
         }) {
        size_t pos = 0;
        while ((pos = s.find(p.begin, pos)) != std::string::npos) {
            size_t start = pos;
            size_t contentStart = pos + p.beginLen;
            size_t end = s.find(p.end, contentStart);
            if (end == std::string::npos) break;
            std::string content = s.substr(contentStart, end - contentStart);
            for (size_t i = 0; i < content.size(); ) {
                if (i + 2 <= content.size() && content[i] == '\\' && content[i + 1] == '\\') {
                    content.replace(i, 2, " ; ");
                    i += 3;
                } else
                    ++i;
            }
            replaceCommonLatexWithUnicode(content);
            std::string repl = "[ " + content + " ]";
            s.replace(start, end - start + strlen(p.end), repl);
            pos = start + repl.size();
        }
    }
}

/// Escape for Pango markup and apply basic markdown: **bold**, *italic*, # header, - list
std::string markdownToPangoMarkup(std::string text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    text = std::move(out);
    out.clear();
    out.reserve(text.size() * 2);
    bool atLineStart = true;
    bool inBold = false;
    bool inItalic = false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (atLineStart) {
            size_t hashCount = 0;
            while (i + hashCount < text.size() && text[i + hashCount] == '#') ++hashCount;
            if (hashCount > 0 && (i + hashCount >= text.size() || text[i + hashCount] == ' ')) {
                while (hashCount-- > 0) ++i;
                while (i < text.size() && text[i] == ' ') ++i;
                out += "<span size=\"large\" weight=\"bold\">";
                size_t lineEnd = text.find('\n', i);
                if (lineEnd == std::string::npos) lineEnd = text.size();
                while (i < lineEnd) out += text[i++];
                out += "</span>";
                if (i < text.size()) out += '\n';
                atLineStart = true;
                continue;
            }
            if (i < text.size() && text[i] == '-' && (i + 1 >= text.size() || text[i + 1] == ' ')) {
                out += "  • ";
                ++i;
                while (i < text.size() && text[i] == ' ') ++i;
                atLineStart = false;
                continue;
            }
        }
        if (text[i] == '\n') {
            atLineStart = true;
            if (inBold) { out += "</b>"; inBold = false; }
            if (inItalic) { out += "</i>"; inItalic = false; }
            out += '\n';
            continue;
        }
        atLineStart = false;
        if (i + 1 < text.size() && text[i] == '*' && text[i + 1] == '*') {
            i += 2;
            if (inBold) { out += "</b>"; inBold = false; }
            else { out += "<b>"; inBold = true; }
            continue;
        }
        if (text[i] == '*' && !inBold) {
            if (inItalic) { out += "</i>"; inItalic = false; }
            else { out += "<i>"; inItalic = true; }
            continue;
        }
        out += text[i];
    }
    if (inBold) out += "</b>";
    if (inItalic) out += "</i>";
    return out;
}

}  // namespace

ChatMessage::ChatMessage(Role role, std::string text, xoj::latex::LatexRenderer* renderer):
    role(role), text(std::move(text)), renderer(renderer) {}

GtkWidget* ChatMessage::buildWidget() {
    GtkWidget* row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), false);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), false);
    gtk_widget_set_hexpand(row, true);

    GtkWidget* bubble = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(bubble, "chat-bubble");
    gtk_widget_set_hexpand(bubble, true);
    switch (role) {
        case Role::USER:
            gtk_widget_add_css_class(bubble, "chat-user");
            gtk_widget_set_halign(bubble, GTK_ALIGN_END);
            break;
        case Role::ASSISTANT:
            gtk_widget_add_css_class(bubble, "chat-assistant");
            gtk_widget_set_halign(bubble, GTK_ALIGN_START);
            break;
        case Role::SYSTEM:
            gtk_widget_add_css_class(bubble, "chat-system");
            gtk_widget_set_halign(bubble, GTK_ALIGN_CENTER);
            break;
    }

    auto segments = xoj::latex::LatexParser::parse(text);
    xoj::latex::LatexRenderer fallbackRenderer;
    xoj::latex::LatexRenderer* latexRenderer = renderer ? renderer : &fallbackRenderer;

    auto copyLatex = +[](GtkButton* button, gpointer userData) {
        auto* latexText = static_cast<std::string*>(userData);
        if (!latexText) return;
        GtkWidget* widget = GTK_WIDGET(button);
        GtkClipboard* clipboard = gtk_widget_get_clipboard(widget);
        gtk_clipboard_set_text(clipboard, latexText->c_str(), -1);
    };

    constexpr size_t kShortInlineMaxLen = 3;
    std::string pendingText;

    auto flushPendingLabel = [&]() {
        if (pendingText.empty()) return;
        replaceCommonLatexWithUnicode(pendingText);
        std::string markup = markdownToPangoMarkup(pendingText);
        GtkWidget* label = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
        gtk_label_set_wrap(GTK_LABEL(label), true);
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_selectable(GTK_LABEL(label), true);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_widget_set_hexpand(label, true);
        gtk_box_append(GTK_BOX(bubble), label);
        pendingText.clear();
    };

    auto addLatexAsTextLabel = [&](const std::string& latex, bool block) {
        std::string display = latex;
        replaceCommonLatexWithUnicode(display);
        std::string markup = markdownToPangoMarkup(display);
        GtkWidget* label = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
        gtk_label_set_wrap(GTK_LABEL(label), true);
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_selectable(GTK_LABEL(label), true);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_widget_set_hexpand(label, true);
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign(row, block ? GTK_ALIGN_CENTER : GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row), label);
        auto* latexCopy = new std::string(latex);
        GtkWidget* copyButton = gtk_button_new_from_icon_name("edit-copy", GTK_ICON_SIZE_MENU);
        g_signal_connect_data(copyButton, "clicked", G_CALLBACK(copyLatex), latexCopy,
                              +[](gpointer data, GClosure*) { delete static_cast<std::string*>(data); },
                              static_cast<GConnectFlags>(0));
        gtk_box_append(GTK_BOX(row), copyButton);
        gtk_box_append(GTK_BOX(bubble), row);
    };

    for (const auto& segment: segments) {
        if (segment.type == xoj::latex::Segment::Type::TEXT) {
            pendingText += segment.content;
        } else if (segment.type == xoj::latex::Segment::Type::LATEX_INLINE) {
            if (segment.content.size() <= kShortInlineMaxLen) {
                pendingText += segment.content;
            } else {
                flushPendingLabel();
                if (latexRenderer->isConfigured()) {
                    GtkWidget* inlineWidget = latexRenderer->renderInline(segment.content);
                    GtkWidget* latexRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
                    gtk_widget_set_halign(latexRow, GTK_ALIGN_START);
                    gtk_box_append(GTK_BOX(latexRow), inlineWidget);
                    auto* latexCopy = new std::string(segment.content);
                    GtkWidget* copyButton = gtk_button_new_from_icon_name("edit-copy", GTK_ICON_SIZE_MENU);
                    g_signal_connect_data(copyButton, "clicked", G_CALLBACK(copyLatex), latexCopy,
                                          +[](gpointer data, GClosure*) { delete static_cast<std::string*>(data); },
                                          static_cast<GConnectFlags>(0));
                    gtk_box_append(GTK_BOX(latexRow), copyButton);
                    gtk_box_append(GTK_BOX(bubble), latexRow);
                } else {
                    addLatexAsTextLabel(segment.content, false);
                }
            }
        } else {
            flushPendingLabel();
            if (latexRenderer->isConfigured()) {
                GtkWidget* blockWidget = latexRenderer->renderBlock(segment.content);
                GtkWidget* blockBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_widget_set_halign(blockBox, GTK_ALIGN_CENTER);
                gtk_box_append(GTK_BOX(blockBox), blockWidget);
                gtk_box_append(GTK_BOX(bubble), blockBox);

                GtkWidget* codeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
                auto* latexCopy = new std::string(segment.content);
                GtkWidget* copyButton = gtk_button_new_with_label("Copiar LaTeX");
                g_signal_connect_data(copyButton, "clicked", G_CALLBACK(copyLatex), latexCopy,
                                      +[](gpointer data, GClosure*) { delete static_cast<std::string*>(data); },
                                      static_cast<GConnectFlags>(0));
                gtk_box_append(GTK_BOX(codeRow), copyButton);

                gtk_box_append(GTK_BOX(bubble), codeRow);
            } else {
                addLatexAsTextLabel(segment.content, true);
            }
        }
    }

    flushPendingLabel();

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), bubble);
    return row;
}

}  // namespace xoj::chat
