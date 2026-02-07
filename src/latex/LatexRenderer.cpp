/*
 * Xournal++
 *
 * LaTeX renderer (SVG)
 *
 * @license GNU GPLv2 or later
 */

#include "latex/LatexRenderer.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <cairo.h>
#include <librsvg/rsvg.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>

#include "filesystem.h"

#include "control/latex/LatexGenerator.h"
#include "latex/LatexCache.h"
#include "util/PathUtil.h"
#include "util/StringUtils.h"
#include "util/Util.h"
#include "util/gtk4_helper.h"

#include <poppler-document.h>
#include <poppler-page.h>

namespace xoj::latex {
namespace {
std::string clampLatex(std::string text) {
    text = StringUtils::trim(text);
    if (text.size() > 5000) {
        text.resize(5000);
    }
    return text;
}
}

LatexRenderer::LatexRenderer(const LatexSettings& settings, std::string templateText) {
    configure(settings, std::move(templateText));
}

void LatexRenderer::configure(const LatexSettings& settings, std::string templateText) {
    this->settings = settings;
    this->templateText = std::move(templateText);
}

GtkWidget* LatexRenderer::loadImage(const fs::path& path, bool block) const {
    int width = block ? 420 : 180;
    int height = block ? 160 : 48;
    GError* err = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(path.string().c_str(), width, height, true, &err);
    if (!pixbuf) {
        if (err) {
            g_error_free(err);
        }
        return nullptr;
    }
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    return image;
}

bool LatexRenderer::renderSvgToPng(const fs::path& svgPath, const fs::path& pngPath, int width, int height) {
    GError* err = nullptr;
    RsvgHandle* handle = rsvg_handle_new_from_file(svgPath.string().c_str(), &err);
    if (!handle) {
        if (err) g_error_free(err);
        return false;
    }
    rsvg_handle_set_dpi(handle, 96.0);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        g_object_unref(handle);
        return false;
    }
    cairo_t* cr = cairo_create(surface);
    const RsvgRectangle viewport{0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
    GError* renderErr = nullptr;
    gboolean ok = rsvg_handle_render_document(handle, cr, &viewport, &renderErr);
    cairo_destroy(cr);
    g_object_unref(handle);
    if (!ok || renderErr) {
        if (renderErr) g_error_free(renderErr);
        cairo_surface_destroy(surface);
        return false;
    }
    ok = (cairo_surface_write_to_png(surface, pngPath.string().c_str()) == CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(surface);
    return ok;
}

// Renders LaTeX to PNG by calling an external "LaTeX→SVG" binary (e.g. MicroTeX in headless mode).
// Expected CLI: <binary> -headless -input="<latex>" -output=<path>. SVG is then converted to PNG with librsvg.
bool LatexRenderer::renderViaLatex2Svg(const std::string& latex, bool block, const fs::path& pngPath,
                                       std::string& error) const {
    if (!latex2SvgPath.has_value() || latex2SvgPath->empty() || !fs::exists(*latex2SvgPath)) {
        return false;
    }
    fs::path tmpDir = Util::getTmpDirSubfolder("chat-latex");
    Util::ensureFolderExists(tmpDir);
    fs::path svgPath = tmpDir / "formula.svg";
    gchar* inputQuoted = g_shell_quote(latex.c_str());
    if (!inputQuoted) {
        error = "Failed to quote LaTeX input.";
        return false;
    }
    std::string inputArg = std::string("-input=") + inputQuoted;
    g_free(inputQuoted);
    std::string outputArg = "-output=" + svgPath.string();
    const char* argv[] = {
            latex2SvgPath->string().c_str(),
            "-headless",
            inputArg.c_str(),
            outputArg.c_str(),
            nullptr
    };
    GError* procErr = nullptr;
    GSubprocessLauncher* launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    fs::path binDir = latex2SvgPath->parent_path();
    if (!binDir.empty()) {
        g_subprocess_launcher_set_cwd(launcher, binDir.string().c_str());
    }
    GSubprocess* proc = g_subprocess_launcher_spawnv(launcher, argv, &procErr);
    g_object_unref(launcher);
    if (!proc) {
        if (procErr) {
            error = procErr->message;
            g_error_free(procErr);
        } else {
            error = "Failed to start LaTeX→SVG process.";
        }
        return false;
    }
    gboolean success = g_subprocess_wait_check(proc, nullptr, &procErr);
    g_object_unref(proc);
    if (!success) {
        if (procErr) {
            error = procErr->message;
            g_error_free(procErr);
        } else {
            error = "LaTeX→SVG process failed.";
        }
        return false;
    }
    if (!fs::exists(svgPath)) {
        error = "LaTeX→SVG produced no output file.";
        return false;
    }
    int width = block ? 420 : 180;
    int height = block ? 160 : 48;
    if (!renderSvgToPng(svgPath, pngPath, width, height)) {
        error = "Failed to convert SVG to PNG.";
        return false;
    }
    return true;
}

bool LatexRenderer::renderToPng(const std::string& latex, bool block, const fs::path& pngPath,
                                std::string& error) const {
    if (latex2SvgPath.has_value() && !latex2SvgPath->empty()) {
        if (renderViaLatex2Svg(latex, block, pngPath, error)) {
            return true;
        }
    }
    if (!settings.has_value() || templateText.empty()) {
        error = "LaTeX renderer not configured.";
        return false;
    }

    fs::path texDir = Util::getTmpDirSubfolder("chat-latex");
    Util::ensureFolderExists(texDir);

    std::string texContents = LatexGenerator::templateSub(clampLatex(latex), templateText, textColor);
    fs::path texFilePath = texDir / "tex.tex";
    GError* writeErr = nullptr;
    if (!g_file_set_contents(texFilePath.string().c_str(), texContents.c_str(), static_cast<gssize>(texContents.size()),
                             &writeErr)) {
        if (writeErr) {
            error = writeErr->message;
            g_error_free(writeErr);
        } else {
            error = "Could not save .tex file.";
        }
        return false;
    }

    GSubprocess* proc = nullptr;
    fs::path pdfPath;

    fs::path tectonicPath = Util::getBundledTectonicPath();
    if (!tectonicPath.empty() && fs::exists(tectonicPath)) {
        const char* argv[] = {tectonicPath.string().c_str(), "-o", texDir.string().c_str(), texFilePath.string().c_str(),
                              nullptr};
        GError* procErr = nullptr;
        proc = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDERR_PIPE, &procErr);
        if (!proc) {
            if (procErr) {
                error = procErr->message;
                g_error_free(procErr);
            } else {
                error = "Failed to start Tectonic.";
            }
            return false;
        }

        gboolean ok = g_subprocess_wait_check(proc, nullptr, &procErr);
        if (!ok) {
            if (procErr) {
                error = procErr->message;
                g_error_free(procErr);
            } else {
                error = "Tectonic render failed.";
            }
            g_object_unref(proc);
            return false;
        }
        pdfPath = texDir / "tex.pdf";
    } else {
        LatexGenerator generator(*settings);
        auto result = generator.asyncRun(texDir, texContents);

        if (auto* err = std::get_if<LatexGenerator::GenError>(&result)) {
            error = err->message;
            return false;
        }

        proc = std::get<GSubprocess*>(result);
        if (!proc) {
            error = "Failed to start LaTeX renderer.";
            return false;
        }

        GError* procErr = nullptr;
        gboolean ok = g_subprocess_wait_check(proc, nullptr, &procErr);
        if (!ok) {
            if (procErr) {
                error = procErr->message;
                g_error_free(procErr);
            } else {
                error = "LaTeX render failed.";
            }
            g_object_unref(proc);
            return false;
        }
        pdfPath = texDir / "tex.pdf";
    }

    if (!fs::exists(pdfPath)) {
        error = "LaTeX output not found.";
        if (proc) {
            g_object_unref(proc);
        }
        return false;
    }

    auto uri = Util::toUri(pdfPath);
    if (!uri) {
        error = "Failed to resolve LaTeX output URI.";
        g_object_unref(proc);
        return false;
    }

    GError* pdfErr = nullptr;
    PopplerDocument* doc = poppler_document_new_from_file(uri->c_str(), nullptr, &pdfErr);
    if (!doc) {
        if (pdfErr) {
            error = pdfErr->message;
            g_error_free(pdfErr);
        } else {
            error = "Failed to load LaTeX PDF.";
        }
        g_object_unref(proc);
        return false;
    }

    PopplerPage* page = poppler_document_get_page(doc, 0);
    if (!page) {
        error = "Failed to load LaTeX PDF page.";
        g_object_unref(doc);
        g_object_unref(proc);
        return false;
    }

    double pageWidth = 0.0;
    double pageHeight = 0.0;
    poppler_page_get_size(page, &pageWidth, &pageHeight);
    const double scale = 240.0 / 72.0;
    int width = std::max(1, static_cast<int>(std::ceil(pageWidth * scale)));
    int height = std::max(1, static_cast<int>(std::ceil(pageHeight * scale)));

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        error = "Failed to create render surface.";
        g_object_unref(page);
        g_object_unref(doc);
        g_object_unref(proc);
        return false;
    }

    cairo_t* cr = cairo_create(surface);
    cairo_scale(cr, scale, scale);
    poppler_page_render(page, cr);
    cairo_destroy(cr);

    cairo_status_t pngStatus = cairo_surface_write_to_png(surface, pngPath.string().c_str());
    cairo_surface_destroy(surface);

    g_object_unref(page);
    g_object_unref(doc);
    if (proc) {
        g_object_unref(proc);
    }

    if (pngStatus != CAIRO_STATUS_SUCCESS || !fs::exists(pngPath)) {
        error = "Failed to convert LaTeX output to PNG.";
        return false;
    }

    return true;
}

void LatexRenderer::renderAsync(const std::string& latex, bool block, GtkWidget* image) {
    std::string latexCopy = latex;
    g_object_ref(image);
    std::thread([this, latexCopy, block, image]() {
        std::string error;
        fs::path pngPath = LatexCache::pathFor(latexCopy, block);
        Util::ensureFolderExists(pngPath.parent_path());
        bool ok = renderToPng(latexCopy, block, pngPath, error);
        if (!ok) {
            g_object_unref(image);
            return;
        }

        auto* update = new std::pair<GtkWidget*, std::string>{image, pngPath.string()};
        g_idle_add_full(
                G_PRIORITY_DEFAULT_IDLE,
                +[](gpointer data) -> gboolean {
                    auto* payload = static_cast<std::pair<GtkWidget*, std::string>*>(data);
                    GtkWidget* img = payload->first;
                    GError* err = nullptr;
                    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(payload->second.c_str(), &err);
                    if (pixbuf) {
                        gtk_image_set_from_pixbuf(GTK_IMAGE(img), pixbuf);
                        g_object_unref(pixbuf);
                    } else if (err) {
                        g_error_free(err);
                    }
                    g_object_unref(img);
                    delete payload;
                    return G_SOURCE_REMOVE;
                },
                update, nullptr);
    }).detach();
}

GtkWidget* LatexRenderer::renderOrError(const std::string& latex, bool block) {
    fs::path pngPath = LatexCache::pathFor(latex, block);
    if (fs::exists(pngPath)) {
        GtkWidget* image = loadImage(pngPath, block);
        if (image) {
            gtk_widget_set_halign(image, block ? GTK_ALIGN_CENTER : GTK_ALIGN_START);
            return image;
        }
    }

    GtkWidget* placeholder = gtk_image_new_from_icon_name("image-missing", GTK_ICON_SIZE_DIALOG);
    gtk_widget_add_css_class(placeholder, block ? "chat-latex-block" : "chat-latex-inline");
    gtk_widget_set_halign(placeholder, block ? GTK_ALIGN_CENTER : GTK_ALIGN_START);

    if (isConfigured()) {
        renderAsync(latex, block, placeholder);
    }

    return placeholder;
}

GtkWidget* LatexRenderer::renderInline(const std::string& latex) {
    GtkWidget* widget = renderOrError(latex, false);
    gtk_widget_add_css_class(widget, "chat-latex-inline");
    return widget;
}

GtkWidget* LatexRenderer::renderBlock(const std::string& latex) {
    GtkWidget* widget = renderOrError(latex, true);
    gtk_widget_add_css_class(widget, "chat-latex-block");
    return widget;
}

}  // namespace xoj::latex
