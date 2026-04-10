#pragma once
#include "models.h"
#include <string>
#include <functional>
#include <vector>

// ── Scraper ───────────────────────────────────────────────────────────────────
// Fetches box art from the Libretro CDN in background threads.
// Thread-safe: queue() can be called from the main thread at any time.
// on_complete fires on the main thread via a pending results queue — call
// Scraper::poll() once per frame to dispatch completed callbacks.

namespace Scraper {

// Callback: (game_id, boxart_path)
using Callback = std::function<void(int64_t, const std::string&)>;

// Enqueue a scrape job for this game. No-op if already cached or queued.
void queue(const Game& game, Callback on_complete);

// Call once per frame from the main thread — dispatches completed callbacks.
void poll();

// Returns the cached boxart path for a game id, or "" if not cached.
std::string cached_path(int64_t game_id);

// Shutdown all background threads (call on app exit).
void shutdown();

} // namespace Scraper
