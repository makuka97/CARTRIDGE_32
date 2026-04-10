#include "launcher.h"
#include "config.h"
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>

namespace Launcher {

struct EmulatorInfo { std::string folder; std::string exe; };

static const std::unordered_map<System, EmulatorInfo> EMULATORS = {
    { System::NES,       {"nestopia",  "nestopia.exe"} },
    { System::SNES,      {"snes9x",   "snes9x.exe"} },
    { System::GB,        {"mgba",     "mGBA.exe"} },
    { System::GBC,       {"mgba",     "mGBA.exe"} },
    { System::GBA,       {"mgba",     "mGBA.exe"} },
    { System::Genesis,   {"gens",     "gens.exe"} },
    { System::SMS,       {"gens",     "gens.exe"} },
    { System::Atari2600, {"stella",   "Stella.exe"} },
    { System::Atari7800, {"stella",   "Stella.exe"} },
    { System::Lynx,      {"mednafen", "mednafen.exe"} },
    { System::PCE,       {"mednafen", "mednafen.exe"} },
    { System::C64,       {"vice",     "bin\\x64sc.exe"} },
    { System::N64,       {"mupen64",  "mupen64plus-ui-console.exe"} },
    { System::DOS,       {"dosbox",   "DOSBox.exe"} },
    // New emulators
    { System::NDS,       {"desmume",  "DeSmuME_0.9.11_x86.exe"} },
    { System::PS1,       {"epsxe",    "ePSXe.exe"} },
    // Mednafen covers these — same binary as Lynx/PCE
    { System::GG,        {"mednafen", "mednafen.exe"} },
    { System::NGP,       {"mednafen", "mednafen.exe"} },
    { System::WS,        {"mednafen", "mednafen.exe"} },
    { System::VB,        {"mednafen", "mednafen.exe"} },
    // Saturn — emulator not yet available, placeholder
    // { System::Saturn,  {"yabause",  "yabause.exe"} },
};

static std::string exe_dir() {
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    auto slash = path.find_last_of("/\\");
    return (slash != std::string::npos) ? path.substr(0, slash + 1) : ".\\";
}

static bool file_exists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Strip trailing slash from path
static std::string strip_slash(std::string s) {
    while (!s.empty() && (s.back() == '\\' || s.back() == '/'))
        s.pop_back();
    return s;
}

std::string emulator_path(System sys) {
    auto it = EMULATORS.find(sys);
    if (it == EMULATORS.end()) return "";
    std::string path = exe_dir() + "native\\" + it->second.folder + "\\" + it->second.exe;
    return file_exists(path) ? path : "";
}

bool emulator_available(System sys) {
    return !emulator_path(sys).empty();
}

bool launch(const Game& game) {
    std::string exe = emulator_path(game.system);
    if (exe.empty()) {
        MessageBoxA(nullptr,
            ("No emulator found.\n\nExpected in:\n" + exe_dir() + "native\\").c_str(),
            "Emulator Not Found", MB_OK | MB_ICONWARNING);
        return false;
    }

    std::string work_dir = strip_slash(exe.substr(0, exe.find_last_of("/\\")));
    std::string cmd;

    if (game.system == System::DOS) {
        // dos_conf is the .conf file path written at import time.
        // Mirror Python's _launch_dos(): DOSBox.exe -conf <conf>
        if (game.dos_conf.empty() || !file_exists(game.dos_conf)) {
            MessageBoxA(nullptr,
                "DOSBox .conf not found.\n\n"
                "Try removing and re-importing the game so the "
                "configuration file is rebuilt.",
                "DOS Launch Error", MB_OK | MB_ICONERROR);
            return false;
        }
        cmd = "\"" + exe + "\" -conf \"" + game.dos_conf + "\"";

    } else if (game.system == System::N64) {
        // Strip trailing slash from plugin_dir — critical, a trailing \"
        // breaks the argument parsing making the next arg part of the path
        std::string plugin_dir = strip_slash(work_dir);
        cmd = "\"" + exe + "\""
              " --plugindir \"" + plugin_dir + "\""
              " --datadir \"" + plugin_dir + "\""
              " \"" + game.rom_path + "\"";

    } else {
        cmd = "\"" + exe + "\" \"" + game.rom_path + "\"";
    }

    STARTUPINFOA si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::vector<char> cb(cmd.begin(), cmd.end());
    cb.push_back('\0');

    BOOL ok = CreateProcessA(nullptr, cb.data(), nullptr, nullptr,
                              FALSE, 0, nullptr, work_dir.c_str(), &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "Launch failed (error %lu).\n\nCommand:\n%s",
                 GetLastError(), cmd.c_str());
        MessageBoxA(nullptr, msg, "Launch Failed", MB_OK | MB_ICONERROR);
    }
    return ok != 0;
}

} // namespace Launcher
