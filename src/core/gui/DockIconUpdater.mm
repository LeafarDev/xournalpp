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

#include <dispatch/dispatch.h>
#include <string>

namespace xoj {

static const CGFloat kDockIconSize = 512.0;

void clearDockIcon() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSApp setApplicationIconImage:nil];
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

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        PDFDocument* doc = [[PDFDocument alloc] initWithURL:url];
        if (!doc || [doc pageCount] == 0) {
            return;
        }

        PDFPage* page = [doc pageAtIndex:0];
        if (!page) {
            return;
        }

        CGSize size = CGSizeMake(kDockIconSize, kDockIconSize);
        NSImage* thumbnail = [page thumbnailOfSize:size forBox:kPDFDisplayBoxMediaBox];
        if (!thumbnail) {
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApp setApplicationIconImage:thumbnail];
        });
    });
}

}  // namespace xoj

#endif  // __APPLE__
