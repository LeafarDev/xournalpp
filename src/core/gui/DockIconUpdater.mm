/*
 * Xournal++
 *
 * macOS: update Dock icon from PDF first page using PDFKit.
 *
 * @license GNU GPLv2 or later
 */

#ifdef __APPLE__

#import <AppKit/AppKit.h>
#import <PDFKit/PDFKit.h>

#include <atomic>
#include <dispatch/dispatch.h>
#include <string>

namespace xoj {

static const CGFloat kDockIconSize = 512.0;
static std::atomic<bool> g_iconUpdateCancelled{false};

void cancelDockIconUpdate() { g_iconUpdateCancelled.store(true); }

void clearDockIcon() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSApp setApplicationIconImage:nil];
    });
}

void clearDockBadgeLabel() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSApp dockTile] setBadgeLabel:nil];
    });
}

void setDockBadgeLabel(const std::string& labelUtf8) {
    if (labelUtf8.empty()) {
        clearDockBadgeLabel();
        return;
    }

    NSString* label = [NSString stringWithUTF8String:labelUtf8.c_str()];
    if (!label || [label length] == 0) {
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSApp dockTile] setBadgeLabel:label];
    });
}

void setDockIconFromPdfPath(const std::string& pdfPathUtf8) {
    if (pdfPathUtf8.empty()) {
        clearDockIcon();
        return;
    }

    NSString* path = [NSString stringWithUTF8String:pdfPathUtf8.c_str()];
    if (!path || [path length] == 0) {
        return;
    }

    NSURL* url = [NSURL fileURLWithPath:path];
    if (!url) {
        return;
    }

    g_iconUpdateCancelled.store(false);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        if (g_iconUpdateCancelled.load()) {
            return;
        }

        PDFDocument* doc = [[PDFDocument alloc] initWithURL:url];
        if (!doc || [doc pageCount] == 0) {
            return;
        }

        if (g_iconUpdateCancelled.load()) {
            return;
        }

        PDFPage* page = [doc pageAtIndex:0];
        if (!page) {
            return;
        }

        CGSize size = CGSizeMake(kDockIconSize, kDockIconSize);
        NSImage* thumbnail = [page thumbnailOfSize:size forBox:kPDFDisplayBoxMediaBox];
        if (!thumbnail || g_iconUpdateCancelled.load()) {
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            if (!g_iconUpdateCancelled.load()) {
                [NSApp setApplicationIconImage:thumbnail];
            }
        });
    });
}

}  // namespace xoj

#endif  // __APPLE__
