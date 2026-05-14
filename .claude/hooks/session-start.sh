#!/usr/bin/env bash
# SessionStart hook for Claude Code on web.
#
# Bootstraps the project so the first tool call doesn't have to install
# everything. Idempotent — safe to run on every session start.
#
# Exits 0 always; if a step fails, prints a hint but doesn't block the session.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

log() { printf '[session-start] %s\n' "$*" >&2; }

# 1. JS dependencies — required for npm run typecheck/validate/smoke.
if [ -f JS-Apps/package-lock.json ]; then
  if [ ! -d JS-Apps/node_modules ] \
     || [ JS-Apps/package-lock.json -nt JS-Apps/node_modules/.package-lock.json ]; then
    log "Installing JS dependencies (npm ci)..."
    (cd JS-Apps && npm ci --no-audit --no-fund) \
      || log "WARNING: npm ci failed; \`npm run validate\` will not work until fixed"
  else
    log "JS dependencies already installed."
  fi
fi

# 2. uFBT — only needed when working on C apps. Cheap to attempt; succeeds
# silently if already installed. Skipped if pip itself isn't available.
if command -v pip >/dev/null 2>&1; then
  if ! command -v ufbt >/dev/null 2>&1; then
    log "Installing uFBT (Momentum SDK build tool)..."
    pip install --quiet --upgrade ufbt 2>&1 \
      | tail -n 5 >&2 \
      || log "WARNING: ufbt install failed; C-app builds will not work until fixed"
  else
    log "uFBT already installed."
  fi
fi

# 3. Display the pinned Momentum SDK so it's visible in the session log.
if [ -f .momentum-sdk-version ]; then
  log "Pinned Momentum SDK:"
  sed 's/^/[session-start]   /' .momentum-sdk-version >&2
fi

exit 0
