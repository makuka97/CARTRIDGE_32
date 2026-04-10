#include "importer.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Importer {

// ── Extension → System map ────────────────────────────────────────────────────

static const std::unordered_map<std::string, System> EXT_MAP = {
    {"nes",  System::NES},
    {"smc",  System::SNES},  {"sfc",  System::SNES},  {"snes", System::SNES},
    {"n64",  System::N64},   {"z64",  System::N64},   {"v64",  System::N64},
    {"gb",   System::GB},
    {"gbc",  System::GBC},
    {"gba",  System::GBA},
    {"md",   System::Genesis}, {"gen", System::Genesis}, {"32x", System::Genesis},
    {"sms",  System::SMS},   {"gg",   System::SMS},
    {"a26",  System::Atari2600},
    {"a78",  System::Atari7800},
    {"lnx",  System::Lynx},
    {"pce",  System::PCE},
    {"d64",  System::C64},   {"t64",  System::C64},
    {"prg",  System::C64},   {"crt",  System::C64},
    {"zip",  System::DOS},
    {"nds",  System::NDS},
    {"dsi",  System::NDS},
    {"cue",  System::PS1},   {"bin",  System::PS1},   {"iso",  System::PS1},
    {"pbp",  System::PS1},
    {"gg",   System::GG},
    {"ngp",  System::NGP},   {"ngc",  System::NGP},
    {"ws",   System::WS},    {"wsc",  System::WS},
    {"vb",   System::VB},
    {"ccd",  System::Saturn}, {"mdf",  System::Saturn},
};

// Some extensions are ambiguous — .bin could be Genesis or many others.
// We resolve by checking if it's a valid Genesis ROM header, else Unknown.
static System resolve_bin(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return System::Unknown;
    char buf[512] = {};
    fread(buf, 1, sizeof(buf), f);
    fclose(f);
    // Genesis ROMs have "SEGA" at offset 0x100 (byte-swapped interleaved)
    // or "SEGA MEGA DRIVE" at 0x100 in non-interleaved.
    if (strncmp(buf + 0x100, "SEGA", 4) == 0 ||
        strncmp(buf + 0x100, "EGAS", 4) == 0)
        return System::Genesis;
    return System::Unknown;
}

System detect_system(const std::string& path) {
    // Extract extension
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return System::Unknown;
    std::string ext = path.substr(dot + 1);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == "bin") return resolve_bin(path);

    auto it = EXT_MAP.find(ext);
    return it != EXT_MAP.end() ? it->second : System::Unknown;
}

// ── Name extraction ───────────────────────────────────────────────────────────

std::string make_name(const std::string& path) {
    // Get filename without directory
    auto slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);

    // Strip extension
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);

    // Replace underscores/dots with spaces
    for (auto& c : name) {
        if (c == '_' || c == '.') c = ' ';
    }

    // Trim leading/trailing spaces
    size_t start = name.find_first_not_of(' ');
    size_t end   = name.find_last_not_of(' ');
    if (start == std::string::npos) return "Unknown";
    return name.substr(start, end - start + 1);
}

// ── File hash (simple FNV-1a for duplicate detection) ─────────────────────────

std::string hash_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";

    uint64_t hash = 14695981039346656037ULL;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            hash ^= buf[i];
            hash *= 1099511628211ULL;
        }
    }
    fclose(f);

    char hex[17];
    snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)hash);
    return std::string(hex);
}

// ── DOS zip executable listing ────────────────────────────────────────────────
// Reads the zip central directory to find .exe / .com / .bat entries.
// Pure C — no zlib needed for just listing filenames.

struct ZipEntry {
    uint32_t sig;
    uint16_t version_made, version_needed, flags, compression;
    uint16_t mod_time, mod_date;
    uint32_t crc32, compressed_size, uncompressed_size;
    uint16_t name_len, extra_len, comment_len;
    uint16_t disk_start, int_attrib;
    uint32_t ext_attrib, local_offset;
};

std::vector<std::string> list_dos_executables(const std::string& zip_path) {
    std::vector<std::string> results;
    FILE* f = fopen(zip_path.c_str(), "rb");
    if (!f) return results;

    // Find End of Central Directory record by scanning from end
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    const int EOCD_SIZE = 22;
    const int MAX_COMMENT = 65535;
    long search_start = file_size - EOCD_SIZE - MAX_COMMENT;
    if (search_start < 0) search_start = 0;

    uint32_t cd_offset = 0, cd_size = 0;
    uint16_t cd_count = 0;
    bool found = false;

    fseek(f, search_start, SEEK_SET);
    uint8_t buf[4];
    long pos = search_start;
    while (pos <= file_size - EOCD_SIZE) {
        fread(buf, 1, 4, f);
        if (buf[0]==0x50 && buf[1]==0x4b && buf[2]==0x05 && buf[3]==0x06) {
            // Found EOCD signature
            fseek(f, pos + 8, SEEK_SET);
            fread(&cd_count,  2, 1, f);
            fread(&cd_count,  2, 1, f); // total entries
            fread(&cd_size,   4, 1, f);
            fread(&cd_offset, 4, 1, f);
            found = true;
            break;
        }
        pos++;
        fseek(f, pos, SEEK_SET);
    }

    if (!found) { fclose(f); return results; }

    // Read central directory entries
    fseek(f, cd_offset, SEEK_SET);
    for (int i = 0; i < cd_count; i++) {
        uint8_t sig[4];
        if (fread(sig, 1, 4, f) != 4) break;
        if (sig[0]!=0x50||sig[1]!=0x4b||sig[2]!=0x01||sig[3]!=0x02) break;

        uint8_t header[42];
        if (fread(header, 1, 42, f) != 42) break;

        uint16_t name_len    = header[24] | (header[25] << 8);
        uint16_t extra_len   = header[26] | (header[27] << 8);
        uint16_t comment_len = header[28] | (header[29] << 8);

        std::string name(name_len, '\0');
        fread(&name[0], 1, name_len, f);
        fseek(f, extra_len + comment_len, SEEK_CUR);

        // Check if it's an executable
        std::string lower = name;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.size() >= 4) {
            std::string ext4 = lower.substr(lower.size() - 4);
            if (ext4 == ".exe" || ext4 == ".com" || ext4 == ".bat")
                results.push_back(name);
        }
    }

    fclose(f);

    // Score each exe: lower = better candidate for "main game exe"
    // Prefer: root-level, .exe extension, short name, not setup/install/unins
    auto score = [](const std::string& p) -> int {
        int s = 0;
        // Depth penalty
        s += (int)std::count(p.begin(), p.end(), '/') * 10;
        // Prefer .exe over .com/.bat
        std::string lp = p;
        for (auto& c : lp) c = (char)std::tolower((unsigned char)c);
        if (lp.size() >= 4 && lp.substr(lp.size()-4) == ".com") s += 2;
        if (lp.size() >= 4 && lp.substr(lp.size()-4) == ".bat") s += 4;
        // Penalise known non-game executables
        if (lp.find("setup")  != std::string::npos) s += 20;
        if (lp.find("instal") != std::string::npos) s += 20;
        if (lp.find("unins")  != std::string::npos) s += 20;
        if (lp.find("config") != std::string::npos) s += 10;
        if (lp.find("readme") != std::string::npos) s += 10;
        return s;
    };

    std::sort(results.begin(), results.end(), [&score](const std::string& a, const std::string& b) {
        int sa = score(a), sb = score(b);
        if (sa != sb) return sa < sb;
        return a < b;
    });

    return results;
}

// ── DOS zip extraction ────────────────────────────────────────────────────────

// Create all intermediate directories in a path.
static void mkdir_recursive(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '\\' || path[i] == '/') {
            CreateDirectoryA(path.substr(0, i).c_str(), nullptr);
        }
    }
}

// Extract a zip file to out_dir using PowerShell Expand-Archive (Win10+).
// Waits synchronously up to 30 seconds.
static bool extract_zip(const std::string& zip_path, const std::string& out_dir) {
    mkdir_recursive(out_dir);

    // PowerShell Expand-Archive — silently overwrite, no window.
    std::string cmd =
        "powershell -NoProfile -NonInteractive -Command \""
        "Expand-Archive -LiteralPath '" + zip_path + "' "
        "-DestinationPath '" + out_dir + "' -Force\"";

    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<char> cb(cmd.begin(), cmd.end());
    cb.push_back('\0');

    if (!CreateProcessA(nullptr, cb.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, 30000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// Write a DOSBox .conf that mounts game_dir as C: and launches dos_exe.
// Returns the path to the written .conf, or "" on failure.
// Mirrors Python's write_dosbox_conf() exactly.
static std::string write_dos_conf(const std::string& game_dir, const std::string& dos_exe) {
    // Normalise slashes to backslash for DOSBox autoexec
    std::string exe = dos_exe;
    for (auto& c : exe) if (c == '/') c = '\\';

    // Split into subdirectory and filename (e.g. "subdir\GAME.EXE" → ("subdir", "GAME.EXE"))
    std::string exe_file = exe;
    std::string exe_subdir;
    auto sep = exe.rfind('\\');
    if (sep != std::string::npos) {
        exe_subdir = exe.substr(0, sep);
        exe_file   = exe.substr(sep + 1);
    }

    // conf lives next to the extraction dir: <game_dir>.conf
    std::string conf_path = game_dir + ".conf";
    FILE* f = fopen(conf_path.c_str(), "w");
    if (!f) return "";

    fprintf(f,
        "[sdl]\r\n"
        "fullscreen=true\r\n"
        "fullresolution=desktop\r\n"
        "\r\n"
        "[dosbox]\r\n"
        "machine=svga_s3\r\n"
        "memsize=16\r\n"
        "\r\n"
        "[cpu]\r\n"
        "core=auto\r\n"
        "cycles=auto\r\n"
        "\r\n"
        "[autoexec]\r\n"
        "@echo off\r\n"
        "mount C \"%s\"\r\n"
        "C:\r\n",
        game_dir.c_str()
    );
    if (!exe_subdir.empty())
        fprintf(f, "cd \\%s\r\n", exe_subdir.c_str());
    fprintf(f, "%s\r\n", exe_file.c_str());
    fprintf(f, "exit\r\n");
    fclose(f);

    return conf_path;
}

// ── Import ────────────────────────────────────────────────────────────────────

ImportResult import_rom(const std::string& path) {
    ImportResult r;

    System sys = detect_system(path);
    if (sys == System::Unknown) {
        r.error = "Unrecognised file type";
        return r;
    }
    if (sys == System::DOS) {
        r.error = "Use import_dos() for zip files";
        return r;
    }

    r.game.name     = make_name(path);
    r.game.rom_path = path;
    r.game.system   = sys;
    r.ok            = true;
    return r;
}

// Import a DOS zip: extract it, write a .conf, store the conf path.
// Mirrors Python's import_rom(zip_path, dos_exe) + write_dosbox_conf() flow.
ImportResult import_dos(const std::string& zip_path, const std::string& exe_inside) {
    ImportResult r;

    // Derive extraction directory = zip path with .zip stripped
    std::string game_dir = zip_path;
    {
        std::string lower = game_dir;
        for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
        if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".zip")
            game_dir = game_dir.substr(0, game_dir.size() - 4);
    }

    // Extract only if the directory doesn't already exist
    DWORD attr = GetFileAttributesA(game_dir.c_str());
    bool already_done = (attr != INVALID_FILE_ATTRIBUTES &&
                         (attr & FILE_ATTRIBUTE_DIRECTORY));
    if (!already_done) {
        if (!extract_zip(zip_path, game_dir)) {
            r.error = "Extraction failed. Ensure PowerShell is available (Windows 10+).";
            return r;
        }
    }

    // Write DOSBox conf — always rewrite so a re-import picks up changes
    std::string conf_path = write_dos_conf(game_dir, exe_inside);
    if (conf_path.empty()) {
        r.error = "Could not write DOSBox configuration file.";
        return r;
    }

    r.game.name     = make_name(zip_path);
    r.game.rom_path = zip_path;
    r.game.system   = System::DOS;
    r.game.dos_conf = conf_path;   // ← .conf path, not exe name (matches Python)
    r.ok            = true;
    return r;
}

} // namespace Importer
