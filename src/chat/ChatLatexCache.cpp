/*
 * Xournal++
 *
 * @license GNU GPLv2 or later
 */

#include "chat/ChatLatexCache.h"

#include <glib.h>

#include "util/PathUtil.h"
#include "util/StringUtils.h"

namespace xoj::chat {
namespace {
std::string sha256(const std::string& input) {
    GChecksum* checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, reinterpret_cast<const guchar*>(input.data()), input.size());
    const gchar* digest = g_checksum_get_string(checksum);
    std::string result = digest ? digest : "";
    g_checksum_free(checksum);
    return result;
}
}  // namespace

fs::path ChatLatexCache::getCacheDir() {
    return Util::getCacheSubfolder("chat_latex");
}

fs::path ChatLatexCache::pathFor(const std::string& latex, bool block) {
    std::string key = sha256(StringUtils::trim(latex) + (block ? ":block" : ":inline") + TEMPLATE_VERSION);
    return getCacheDir() / (key + ".pdf");
}

}  // namespace xoj::chat
