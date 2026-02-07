/*
 * Xournal++
 *
 * LaTeX renderer (SVG)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>
#include <string>

#include <gtk/gtk.h>

#include "control/settings/LatexSettings.h"
#include "util/Color.h"
#include "filesystem.h"

namespace xoj::latex {

class LatexRenderer {
public:
    LatexRenderer() = default;
    LatexRenderer(const LatexSettings& settings, std::string templateText);

    void configure(const LatexSettings& settings, std::string templateText);
    /// Set path to a LaTeX→SVG binary (e.g. MicroTeX -headless). When set, chat formulas use it instead of pdflatex.
    void setLatex2SvgPath(fs::path path) { latex2SvgPath = std::move(path); }
    bool isConfigured() const {
        return (latex2SvgPath.has_value() && !latex2SvgPath->empty()) ||
               (settings.has_value() && !templateText.empty());
    }
    GtkWidget* renderInline(const std::string& latex);
    GtkWidget* renderBlock(const std::string& latex);

private:
    GtkWidget* renderOrError(const std::string& latex, bool block);
    GtkWidget* loadImage(const fs::path& path, bool block) const;
    void renderAsync(const std::string& latex, bool block, GtkWidget* image);
    bool renderToPng(const std::string& latex, bool block, const fs::path& pngPath, std::string& error) const;
    bool renderViaLatex2Svg(const std::string& latex, bool block, const fs::path& pngPath, std::string& error) const;
    static bool renderSvgToPng(const fs::path& svgPath, const fs::path& pngPath, int width, int height);

    std::optional<LatexSettings> settings;
    std::string templateText;
    std::optional<fs::path> latex2SvgPath;
    Color textColor = Colors::black;
};

}  // namespace xoj::latex
