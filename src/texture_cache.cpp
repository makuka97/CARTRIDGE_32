#include "texture_cache.h"

#include <SDL_opengl.h>
#include <windows.h>
#include <wincodec.h>   // WIC - Windows Imaging Component, Vista+

#include <unordered_map>
#include <list>
#include <string>
#include <vector>
#include <cstdint>


namespace TextureCache {

static const int MAX_TEXTURES = 256;

struct Entry { GLuint tex_id = 0; };

static std::list<std::string>                                            g_lru;
static std::unordered_map<std::string, std::list<std::string>::iterator> g_iter_map;
static std::unordered_map<std::string, Entry>                            g_cache;

static IWICImagingFactory* g_wic = nullptr;

static void ensure_wic() {
    if (g_wic) return;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                     CLSCTX_INPROC_SERVER,
                     IID_IWICImagingFactory,
                     reinterpret_cast<void**>(&g_wic));
}

static void evict_lru() {
    if (g_lru.empty()) return;
    const std::string& oldest = g_lru.back();
    auto it = g_cache.find(oldest);
    if (it != g_cache.end()) {
        glDeleteTextures(1, &it->second.tex_id);
        g_cache.erase(it);
    }
    g_iter_map.erase(oldest);
    g_lru.pop_back();
}

static bool wic_load(const std::string& path,
                     std::vector<uint8_t>& out, int& w, int& h) {
    ensure_wic();
    if (!g_wic) return false;

    // Convert to wide
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> wpath(wlen);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

    IWICBitmapDecoder*     decoder   = nullptr;
    IWICBitmapFrameDecode* frame     = nullptr;
    IWICFormatConverter*   converter = nullptr;

    HRESULT hr = g_wic->CreateDecoderFromFilename(
        wpath.data(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return false;

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { decoder->Release(); return false; }

    hr = g_wic->CreateFormatConverter(&converter);
    if (FAILED(hr)) { frame->Release(); decoder->Release(); return false; }

    hr = converter->Initialize(frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        converter->Release(); frame->Release(); decoder->Release();
        return false;
    }

    UINT uw = 0, uh = 0;
    converter->GetSize(&uw, &uh);
    w = (int)uw; h = (int)uh;
    out.resize(w * h * 4);

    hr = converter->CopyPixels(nullptr, w * 4, (UINT)out.size(), out.data());

    converter->Release();
    frame->Release();
    decoder->Release();

    return SUCCEEDED(hr);
}

ImTextureID load(const std::string& path) {
    if (path.empty()) return (ImTextureID)0;

    // Cache hit
    auto it = g_cache.find(path);
    if (it != g_cache.end()) {
        auto lit = g_iter_map.find(path);
        if (lit != g_iter_map.end()) {
            g_lru.erase(lit->second);
            g_lru.push_front(path);
            lit->second = g_lru.begin();
        }
        return (ImTextureID)(uintptr_t)it->second.tex_id;
    }

    while ((int)g_cache.size() >= MAX_TEXTURES)
        evict_lru();

    std::vector<uint8_t> pixels;
    int w = 0, h = 0;
    if (!wic_load(path, pixels, w, h)) return (ImTextureID)0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    Entry e; e.tex_id = tex;
    g_cache[path] = e;
    g_lru.push_front(path);
    g_iter_map[path] = g_lru.begin();

    return (ImTextureID)(uintptr_t)tex;
}

void evict(const std::string& path) {
    auto it = g_cache.find(path);
    if (it == g_cache.end()) return;
    glDeleteTextures(1, &it->second.tex_id);
    g_cache.erase(it);
    auto lit = g_iter_map.find(path);
    if (lit != g_iter_map.end()) {
        g_lru.erase(lit->second);
        g_iter_map.erase(lit);
    }
}

void clear() {
    for (auto& kv : g_cache)
        glDeleteTextures(1, &kv.second.tex_id);
    g_cache.clear();
    g_lru.clear();
    g_iter_map.clear();
    if (g_wic) { g_wic->Release(); g_wic = nullptr; }
}

} // namespace TextureCache
