#include "texture_cache.h"

#include <SDL_opengl.h>

#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#else
// Linux: use libpng for PNG loading
#include <png.h>
#include <cstring>
#endif

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

#ifdef _WIN32
static IWICImagingFactory* g_wic = nullptr;

static void ensure_wic() {
    if (g_wic) return;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_IWICImagingFactory, reinterpret_cast<void**>(&g_wic));
}

static bool platform_load(const std::string& path,
                           std::vector<uint8_t>& out, int& w, int& h) {
    ensure_wic();
    if (!g_wic) return false;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> wpath(wlen);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

    IWICBitmapDecoder*     decoder   = nullptr;
    IWICBitmapFrameDecode* frame     = nullptr;
    IWICFormatConverter*   converter = nullptr;

    if (FAILED(g_wic->CreateDecoderFromFilename(wpath.data(), nullptr,
               GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    if (FAILED(decoder->GetFrame(0, &frame))) { decoder->Release(); return false; }
    if (FAILED(g_wic->CreateFormatConverter(&converter))) {
        frame->Release(); decoder->Release(); return false;
    }
    if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
               WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        converter->Release(); frame->Release(); decoder->Release(); return false;
    }
    UINT uw = 0, uh = 0; converter->GetSize(&uw, &uh);
    w = (int)uw; h = (int)uh;
    out.resize(w * h * 4);
    HRESULT hr = converter->CopyPixels(nullptr, w * 4, (UINT)out.size(), out.data());
    converter->Release(); frame->Release(); decoder->Release();
    return SUCCEEDED(hr);
}

#else
// Linux: libpng loader
static bool platform_load(const std::string& path,
                           std::vector<uint8_t>& out, int& w, int& h) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    uint8_t sig[8];
    if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8)) {
        fclose(fp); return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                              nullptr, nullptr, nullptr);
    if (!png) { fclose(fp); return false; }

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); fclose(fp); return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr); fclose(fp); return false;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    w = (int)png_get_image_width(png, info);
    h = (int)png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth  = png_get_bit_depth(png, info);

    // Normalise to RGBA8
    if (bit_depth == 16)            png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB  ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_read_update_info(png, info);

    out.resize(w * h * 4);
    std::vector<png_bytep> rows(h);
    for (int y = 0; y < h; y++)
        rows[y] = out.data() + y * w * 4;
    png_read_image(png, rows.data());
    png_destroy_read_struct(&png, &info, nullptr);
    fclose(fp);
    return true;
}
#endif

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

ImTextureID load(const std::string& path) {
    if (path.empty()) return (ImTextureID)0;

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

    while ((int)g_cache.size() >= MAX_TEXTURES) evict_lru();

    std::vector<uint8_t> pixels;
    int w = 0, h = 0;
    if (!platform_load(path, pixels, w, h)) return (ImTextureID)0;

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
    for (auto& kv : g_cache) glDeleteTextures(1, &kv.second.tex_id);
    g_cache.clear(); g_lru.clear(); g_iter_map.clear();
#ifdef _WIN32
    if (g_wic) { g_wic->Release(); g_wic = nullptr; }
#endif
}

} // namespace TextureCache
