/*
 * Xournal++
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ChatLatexCompiler.h"

#include <regex>

#include <glib.h>
#include <gio/gio.h>

#include "chat/ChatLatexCache.h"
#include "control/latex/LatexGenerator.h"
#include "control/settings/LatexSettings.h"
#include "util/PathUtil.h"
#include "util/StringUtils.h"
#include "util/Color.h"

namespace xoj::chat {
namespace {

constexpr const char* CHAT_TEMPLATE = R"(\documentclass[preview]{article}
\usepackage{amsmath}
\usepackage{amssymb}
\usepackage[margin=5pt]{geometry}
\pagestyle{empty}
\setlength{\parindent}{0pt}
\begin{document}
%%XPP_TOOL_INPUT%%
\end{document}
)";

std::string clampLatex(std::string text) {
    text = StringUtils::trim(text);
    if (text.size() > 5000) {
        text.resize(5000);
    }
    return text;
}

/** Remove dangerous commands that could read/write files or run shell. */
std::string sanitizeLatex(std::string latex) {
    // Remove \input{...}, \include{...}, \write{...}, \immediate\write{...}, \openin, \openout, \message{...}, \errmessage{...}, \special{...}
    static const std::regex dangerous(
        R"(\\(?:input|include|write|immediate|openin|openout|message|errmessage|special)\s*(\{[^}]*\}|[^\s]*))",
        std::regex_constants::icase);
    return std::regex_replace(latex, dangerous, "% removed");
}

}  // namespace

CompileResult compileToPdf(const LatexSettings& settings, const std::string& latex, bool block) {
    std::string safe = sanitizeLatex(clampLatex(latex));
    if (block) {
        // LATEX_BLOCK from $$...$$ or \[...\] is raw math; wrap in \[...\] unless already \begin{...}
        if (safe.find("\\begin{") != 0) {
            safe = "\\[" + safe + "\\]";
        }
    } else {
        // LATEX_INLINE content (e.g. "x^2") must be wrapped in $...$ for valid LaTeX
        safe = "$" + safe + "$";
    }
    fs::path cachePath = ChatLatexCache::pathFor(safe, block);
    if (fs::exists(cachePath)) {
        return cachePath;
    }

    fs::path cacheDir = ChatLatexCache::getCacheDir();
    try {
        fs::create_directories(cacheDir);
    } catch (const fs::filesystem_error& e) {
        return std::string("Could not create cache dir: ") + e.what();
    }

    fs::path texDir = Util::getTmpDirSubfolder("chat-latex");
    try {
        fs::create_directories(texDir);
    } catch (const fs::filesystem_error& e) {
        return std::string("Could not create temp dir: ") + e.what();
    }

    Color textColor = Colors::black;
    std::string texContents = LatexGenerator::templateSub(safe, CHAT_TEMPLATE, textColor);
    fs::path texFilePath = texDir / "tex.tex";
    GError* writeErr = nullptr;
    if (!g_file_set_contents(texFilePath.string().c_str(), texContents.c_str(),
                             static_cast<gssize>(texContents.size()), &writeErr)) {
        std::string err = writeErr ? writeErr->message : "Could not save .tex file.";
        if (writeErr) {
            g_error_free(writeErr);
        }
        return err;
    }

    GSubprocess* proc = nullptr;
    fs::path pdfPath;

    fs::path tectonicPath = Util::getBundledTectonicPath();
    if (!tectonicPath.empty() && fs::exists(tectonicPath)) {
        std::string tectonicStr = tectonicPath.string();
        std::string texDirStr = texDir.string();
        std::string texFilePathStr = texFilePath.string();
        const char* argv[] = {tectonicStr.c_str(), "-o", texDirStr.c_str(), texFilePathStr.c_str(), nullptr};
        GError* procErr = nullptr;
        proc = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDERR_PIPE, &procErr);
        if (!proc) {
            std::string err = procErr ? procErr->message : "Failed to start Tectonic.";
            if (procErr) {
                g_error_free(procErr);
            }
            return err;
        }
        GInputStream* stderrStream = g_subprocess_get_stderr_pipe(proc);
        std::string stderrOutput;
        if (stderrStream) {
            char buf[1024];
            GError* readErr = nullptr;
            for (gssize n; (n = g_input_stream_read(stderrStream, buf, sizeof(buf) - 1, nullptr, &readErr)) > 0;) {
                buf[n] = '\0';
                stderrOutput.append(buf);
            }
            if (readErr) {
                g_error_free(readErr);
            }
        }
        gboolean ok = g_subprocess_wait_check(proc, nullptr, &procErr);
        if (!ok) {
            std::string err = procErr ? procErr->message : "Tectonic render failed.";
            std::string trimmed = StringUtils::trim(stderrOutput);
            if (!trimmed.empty()) {
                // Use last ~500 chars of stderr (usually contains the error)
                if (trimmed.size() > 500) {
                    trimmed = "..." + trimmed.substr(trimmed.size() - 497);
                }
                err += ": " + trimmed;
            }
            if (procErr) {
                g_error_free(procErr);
            }
            g_object_unref(proc);
            return err;
        }
        pdfPath = texDir / "tex.pdf";
    } else {
        LatexGenerator generator(settings);
        auto result = generator.asyncRun(texDir, texContents);
        if (auto* err = std::get_if<LatexGenerator::GenError>(&result)) {
            return err->message;
        }
        proc = std::get<GSubprocess*>(result);
        if (!proc) {
            return std::string("Failed to start LaTeX renderer.");
        }
        GError* procErr = nullptr;
        gboolean ok = g_subprocess_wait_check(proc, nullptr, &procErr);
        if (!ok) {
            std::string err = procErr ? procErr->message : "LaTeX render failed.";
            if (procErr) {
                g_error_free(procErr);
            }
            g_object_unref(proc);
            return err;
        }
        pdfPath = texDir / "tex.pdf";
    }

    if (!fs::exists(pdfPath)) {
        if (proc) {
            g_object_unref(proc);
        }
        return std::string("LaTeX output not found.");
    }

    try {
        fs::copy_file(pdfPath, cachePath, fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& e) {
        if (proc) {
            g_object_unref(proc);
        }
        return std::string("Could not copy to cache: ") + e.what();
    }
    if (proc) {
        g_object_unref(proc);
    }
    return cachePath;
}

}  // namespace xoj::chat
