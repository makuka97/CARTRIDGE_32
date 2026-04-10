#pragma once
#include <string>
#include <cstdint>
#include <imgui.h>

// ── TextureCache ──────────────────────────────────────────────────────────────
// Loads PNG files from disk into OpenGL textures using GDI+.
// Caches by file path. LRU eviction at MAX_TEXTURES entries.
// All calls must be made from the main (OpenGL) thread.

namespace TextureCache {

// Returns an ImTextureID for the given image path.
// Returns 0 if the file doesn't exist or can't be loaded.
ImTextureID load(const std::string& path);

// Evict a specific path from the cache.
void evict(const std::string& path);

// Release all textures (call on shutdown).
void clear();

} // namespace TextureCache
