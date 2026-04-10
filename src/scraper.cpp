#include "scraper.h"
#include "models.h"

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <functional>
#include <algorithm>
#include <regex>

namespace Scraper {

// ── Libretro repo names (mirrors Python LIBRETRO_REPOS) ───────────────────────

static const std::unordered_map<System, std::string> LIBRETRO_REPOS = {
    { System::NES,       "Nintendo_-_Nintendo_Entertainment_System" },
    { System::SNES,      "Nintendo_-_Super_Nintendo_Entertainment_System" },
    { System::N64,       "Nintendo_-_Nintendo_64" },
    { System::GB,        "Nintendo_-_Game_Boy" },
    { System::GBC,       "Nintendo_-_Game_Boy_Color" },
    { System::GBA,       "Nintendo_-_Game_Boy_Advance" },
    { System::Genesis,   "Sega_-_Mega_Drive_-_Genesis" },
    { System::SMS,       "Sega_-_Master_System_-_Mark_III" },
    { System::Atari2600, "Atari_-_2600" },
    { System::Atari7800, "Atari_-_7800" },
    { System::Lynx,      "Atari_-_Lynx" },
    { System::PCE,       "NEC_-_PC_Engine_-_TurboGrafx_16" },
    { System::C64,       "Commodore_-_64" },
    { System::DOS,       "DOS" },
    { System::NDS,       "Nintendo_-_Nintendo_DS" },
    { System::PS1,       "Sony_-_PlayStation" },
    { System::GG,        "Sega_-_Game_Gear" },
    { System::NGP,       "SNK_-_Neo_Geo_Pocket_Color" },
    { System::WS,        "Bandai_-_WonderSwan_Color" },
    { System::VB,        "Nintendo_-_Virtual_Boy" },
    { System::Saturn,    "Sega_-_Saturn" },
};

// ── GoodSet region codes → No-Intro region names ──────────────────────────────

static const std::unordered_map<std::string, std::string> GOODSET_REGIONS = {
    {"U","USA"},{"E","Europe"},{"J","Japan"},{"G","Germany"},
    {"F","France"},{"S","Spain"},{"I","Italy"},{"A","Australia"},
    {"C","Canada"},{"B","Brazil"},{"K","Korea"},
    {"1","USA"},{"UE","USA, Europe"},{"JU","Japan, USA"},
    {"JE","Japan, Europe"},{"JUE","Japan, USA, Europe"},
    {"EU","Europe, USA"},
};

// ── URL encoding (mirrors Python url_encode) ──────────────────────────────────

static std::string url_encode(const std::string& s) {
    static const char* safe =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    std::string out;
    for (unsigned char c : s) {
        if (strchr(safe, c)) {
            out += c;
        } else if (c == ' ') {
            out += "%20";
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

// ── Name variant building (mirrors Python build_name_variants) ────────────────

static std::string strip_tags(const std::string& s) {
    // Remove (tag) and [tag] from a filename stem
    std::string out;
    int depth_p = 0, depth_b = 0;
    for (char c : s) {
        if      (c == '(') { depth_p++; continue; }
        else if (c == ')') { if (depth_p > 0) depth_p--; continue; }
        else if (c == '[') { depth_b++; continue; }
        else if (c == ']') { if (depth_b > 0) depth_b--; continue; }
        if (depth_p == 0 && depth_b == 0)
            out += c;
    }
    // Trim
    size_t a = out.find_first_not_of(' ');
    size_t b = out.find_last_not_of(' ');
    return (a == std::string::npos) ? "" : out.substr(a, b - a + 1);
}

static bool is_goodset(const std::string& stem) {
    // Has (U), (J), (E), [!], (V1.x) etc.
    return stem.find("(U)") != std::string::npos ||
           stem.find("(J)") != std::string::npos ||
           stem.find("(E)") != std::string::npos ||
           stem.find("[!]") != std::string::npos ||
           stem.find("(V1") != std::string::npos ||
           stem.find("(V2") != std::string::npos ||
           (stem.find('(') != std::string::npos &&
            stem.find("USA") == std::string::npos &&
            stem.find("Europe") == std::string::npos);
}

static std::vector<std::string> build_name_variants(
        const std::string& rom_path, const std::string& display_name) {

    std::vector<std::string> seen;
    auto add = [&](const std::string& v) {
        if (!v.empty() && std::find(seen.begin(), seen.end(), v) == seen.end())
            seen.push_back(v);
    };

    // Extract stem from path
    auto slash = rom_path.find_last_of("/\\");
    std::string filename = (slash == std::string::npos)
        ? rom_path : rom_path.substr(slash + 1);
    auto dot = filename.rfind('.');
    std::string stem = (dot == std::string::npos)
        ? filename : filename.substr(0, dot);

    std::string base = strip_tags(stem);

    if (is_goodset(stem)) {
        // Extract region code from first (X) tag
        auto p0 = stem.find('(');
        while (p0 != std::string::npos) {
            auto p1 = stem.find(')', p0);
            if (p1 == std::string::npos) break;
            std::string code = stem.substr(p0 + 1, p1 - p0 - 1);
            auto it = GOODSET_REGIONS.find(code);
            if (it != GOODSET_REGIONS.end())
                add(base + " (" + it->second + ")");
            p0 = stem.find('(', p1);
        }
        // Common fallbacks
        for (const auto& r : {"USA", "Europe", "Japan", "USA, Europe", "Japan, USA"})
            add(base + " (" + r + ")");
    } else {
        // Already No-Intro or clean
        add(stem);
        // Strip trailing region if present e.g. "Zelda (USA)" → "Zelda"
        if (!base.empty()) add(base);
    }

    // Display name fallback
    add(display_name);

    // "The X" ↔ "X, The" swap
    std::vector<std::string> extra;
    for (const auto& name : seen) {
        if (name.substr(0, 4) == "The ") extra.push_back(name.substr(4) + ", The");
        else if (name.substr(0, 2) == "A " ) extra.push_back(name.substr(2) + ", A");
        auto the_pos = name.rfind(", The");
        if (the_pos != std::string::npos)
            extra.push_back("The " + name.substr(0, the_pos));
    }
    for (const auto& e : extra) add(e);

    return seen;
}

// ── Boxart directory ──────────────────────────────────────────────────────────

static void make_dirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/' || path[i] == '\\') {
#ifdef _WIN32
            CreateDirectoryA(path.substr(0, i).c_str(), nullptr);
#else
            mkdir(path.substr(0, i).c_str(), 0755);
#endif
        }
    }
}

static std::string boxart_dir() {
#ifdef _WIN32
    char path[MAX_PATH] = {};
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path);
    std::string d = std::string(path) + "\\CARTRIDGE32\\boxart\\";
    make_dirs(d);
    return d;
#else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    std::string base = xdg ? xdg : (std::string(getenv("HOME") ? getenv("HOME") : ".") + "/.config");
    std::string d = base + "/cartridge32/boxart/";
    make_dirs(d);
    return d;
#endif
}

static std::string cached_png(int64_t game_id) {
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%lld", (long long)game_id);
    return boxart_dir() + id_str + ".png";
}

// Sidecar: stores the game name the PNG was downloaded for.
// Used to detect stale cache files after a DB wipe+reimport.
static std::string sidecar_path(int64_t game_id) {
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%lld", (long long)game_id);
    return boxart_dir() + id_str + ".name";
}

static std::string read_sidecar(int64_t game_id) {
    FILE* f = fopen(sidecar_path(game_id).c_str(), "r");
    if (!f) return "";
    char buf[512] = {};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    // strip newline
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

static void write_sidecar(int64_t game_id, const std::string& name) {
    FILE* f = fopen(sidecar_path(game_id).c_str(), "w");
    if (f) { fprintf(f, "%s\n", name.c_str()); fclose(f); }
}

// ── WinINet HTTP GET → file ───────────────────────────────────────────────────

#ifdef _WIN32
static bool http_get_to_file(const std::string& url, const std::string& out_path) {
    HINTERNET hInet = InternetOpenA("CARTRIDGE32/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) return false;
    HINTERNET hUrl = InternetOpenUrlA(hInet, url.c_str(), nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
        INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_UI, 0);
    if (!hUrl) { InternetCloseHandle(hInet); return false; }
    DWORD status = 0, status_size = sizeof(status);
    HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                   &status, &status_size, nullptr);
    if (status != 200) { InternetCloseHandle(hUrl); InternetCloseHandle(hInet); return false; }
    std::vector<uint8_t> data;
    uint8_t buf[4096]; DWORD bytes_read = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &bytes_read) && bytes_read > 0)
        data.insert(data.end(), buf, buf + bytes_read);
    InternetCloseHandle(hUrl); InternetCloseHandle(hInet);
    if (data.size() < 1024) return false;
    FILE* f = fopen(out_path.c_str(), "wb");
    if (!f) return false;
    fwrite(data.data(), 1, data.size(), f); fclose(f);
    return true;
}
#else
// Linux: libcurl
static size_t curl_write(void* ptr, size_t size, size_t nmemb, std::vector<uint8_t>* buf) {
    size_t total = size * nmemb;
    buf->insert(buf->end(), (uint8_t*)ptr, (uint8_t*)ptr + total);
    return total;
}
static bool http_get_to_file(const std::string& url, const std::string& out_path) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    std::vector<uint8_t> data;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CARTRIDGE32/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK || http_code != 200 || data.size() < 1024) return false;
    FILE* f = fopen(out_path.c_str(), "wb");
    if (!f) return false;
    fwrite(data.data(), 1, data.size(), f); fclose(f);
    return true;
}
#endif

// ── Thread-safe result queue ──────────────────────────────────────────────────

struct Result {
    int64_t     game_id;
    std::string path;
    Callback    callback;
};

static std::mutex              g_results_mutex;
static std::vector<Result>     g_results;
static std::mutex              g_queued_mutex;
static std::unordered_set<int64_t> g_queued;

static void push_result(int64_t id, const std::string& path, Callback cb) {
    std::lock_guard<std::mutex> lk(g_results_mutex);
    g_results.push_back({id, path, std::move(cb)});
}

// ── Worker ────────────────────────────────────────────────────────────────────

static void worker(Game game, Callback on_complete) {
    auto repo_it = LIBRETRO_REPOS.find(game.system);
    if (repo_it == LIBRETRO_REPOS.end()) return;

    const std::string& repo = repo_it->second;

    // Replace underscores with spaces for CDN path
    std::string cdn_system = repo;
    for (auto& c : cdn_system) if (c == '_') c = ' ';

    std::string out_path = cached_png(game.id);

    // Already cached on disk? Verify it was downloaded for THIS game (sidecar check).
    #ifdef _WIN32
    bool _exists1 = GetFileAttributesA(out_path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat _st1; bool _exists1 = (stat(out_path.c_str(), &_st1) == 0);
#endif
    if (_exists1) {
        std::string stored_name = read_sidecar(game.id);
        if (stored_name == game.name) {
            // Cache hit - correct game
            push_result(game.id, out_path, on_complete);
            return;
        }
        // Stale file - delete and re-download
#ifdef _WIN32
        DeleteFileA(out_path.c_str());
        DeleteFileA(sidecar_path(game.id).c_str());
#else
        unlink(out_path.c_str());
        unlink(sidecar_path(game.id).c_str());
#endif
    }

    auto variants = build_name_variants(game.rom_path, game.name);

    // Two CDN base URLs — primary CDN then GitHub raw fallback
    // Format: {base}/{system}/Named_Boxarts/{name}.png
    struct CDN { const char* fmt; bool use_spaces; };
    static const CDN CDNS[] = {
        { "https://thumbnails.libretro.com/%s/Named_Boxarts/%s.png",              true  },
        { "https://raw.githubusercontent.com/libretro-thumbnails/%s/master/Named_Boxarts/%s.png", false },
    };

    for (const auto& variant : variants) {
        for (const auto& cdn : CDNS) {
            const std::string& sys_part = cdn.use_spaces ? cdn_system : repo;
            char url[2048];
            snprintf(url, sizeof(url), cdn.fmt,
                     url_encode(sys_part).c_str(),
                     url_encode(variant).c_str());

            if (http_get_to_file(url, out_path)) {
                write_sidecar(game.id, game.name);
                push_result(game.id, out_path, on_complete);
                return;
            }
        }
    }
    // No art found — not an error, just no callback
}

// ── Public API ────────────────────────────────────────────────────────────────

void queue(const Game& game, Callback on_complete) {
    // Already cached on disk? Verify sidecar matches game name.
    std::string path = cached_png(game.id);
    #ifdef _WIN32
    bool _exists2 = GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat _st2; bool _exists2 = (stat(path.c_str(), &_st2) == 0);
#endif
    if (_exists2) {
        if (read_sidecar(game.id) == game.name) {
            push_result(game.id, path, on_complete);
            return;
        }
        // Stale — delete and fall through to re-download
#ifdef _WIN32
        DeleteFileA(path.c_str());
        DeleteFileA(sidecar_path(game.id).c_str());
#else
        unlink(path.c_str());
        unlink(sidecar_path(game.id).c_str());
#endif
    }

    // Already queued?
    {
        std::lock_guard<std::mutex> lk(g_queued_mutex);
        if (g_queued.count(game.id)) return;
        g_queued.insert(game.id);
    }

    // Launch detached thread
    std::thread t(worker, game, on_complete);
    t.detach();
}

void poll() {
    std::vector<Result> batch;
    {
        std::lock_guard<std::mutex> lk(g_results_mutex);
        batch.swap(g_results);
    }
    for (auto& r : batch) {
        r.callback(r.game_id, r.path);
        // Remove from queued set so it can be re-queued if needed
        std::lock_guard<std::mutex> lk(g_queued_mutex);
        g_queued.erase(r.game_id);
    }
}

std::string cached_path(int64_t game_id) {
    std::string p = cached_png(game_id);
#ifdef _WIN32
    return (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) ? p : "";
#else
    struct stat _cst; return (stat(p.c_str(), &_cst) == 0) ? p : "";
#endif
}

void shutdown() {
    // Detached threads finish on their own — just clear state
    std::lock_guard<std::mutex> lk(g_queued_mutex);
    g_queued.clear();
}

} // namespace Scraper
