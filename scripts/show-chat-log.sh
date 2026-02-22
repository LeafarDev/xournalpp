#!/bin/sh
# Run Xournal++ with chat debug logging.
# Usage: ./scripts/show-chat-log.sh [path/to/xournalpp]
#
# Logs appear on stderr. Pipe to a file to save:
#   G_MESSAGES_DEBUG=xopp ./scripts/show-chat-log.sh 2>&1 | tee chat-debug.log

EXE="${1:-./build/xournalpp}"
export G_MESSAGES_DEBUG="${G_MESSAGES_DEBUG:-xopp}"
exec "$EXE" "$@"
