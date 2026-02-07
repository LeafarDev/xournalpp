/*
 * Xournal++
 *
 * LaTeX cache helper
 *
 * @license GNU GPLv2 or later
 */

#include "latex/LatexCache.h"

#include <fstream>

#include <glib.h>

#include "util/PathUtil.h"
#include "util/StringUtils.h"

namespace xoj::latex {
namespace {
std::string sha256(const std::string& input) {
    GChecksum* checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, reinterpret_cast<const guchar*>(input.data()), input.size());
    const gchar* digest = g_checksum_get_string(checksum);
    std::string result = digest ? digest : "";
    g_checksum_free(checksum);
    return result;
}
}

fs::path LatexCache::getCacheDir() {
    return Util::getConfigSubfolder("latex-cache");
}

fs::path LatexCache::pathFor(const std::string& latex, bool block) {
    std::string key = sha256(latex + (block ? ":block" : ":inline"));
    return getCacheDir() / (key + ".png");
}

}  // namespace xoj::latex
