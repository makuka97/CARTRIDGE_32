#include "launcher.h"
#include "config.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <SDL.h>
#endif

#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>

namespace Launcher {

struct EmulatorInfo { std::string folder; std::string exe; };

static const std::unordered_map<System, EmulatorInfo> EMULATORS = {
    { System::NES,       {"nestopia",  "nestopia.exe"} },
#ifdef _WIN32
    { System::SNES,      {"snes9x",   "snes9x.exe"} },
#else
    { System::SNES,      {"mednafen", "mednafen"} },
#endif
#ifdef _WIN32
    { System::GB,        {"mgba",     "mGBA.exe"} },
    { System::GBC,       {"mgba",     "mGBA.exe"} },
    { System::GBA,       {"mgba",     "mGBA.exe"} },
#else
    { System::GB,        {"mgba",     "mgba-qt"} },
    { System::GBC,       {"mgba",     "mgba-qt"} },
    { System::GBA,       {"mgba",     "mgba-qt"} },
#endif
#ifdef _WIN32
    { System::Genesis,   {"gens",     "gens.exe"} },
    { System::SMS,       {"gens",     "gens.exe"} },
#else
    { System::Genesis,   {"mednafen", "mednafen"} },
    { System::SMS,       {"mednafen", "mednafen"} },
#endif
    { System::Atari2600, {"stella",   "Stella.exe"} },
    { System::Atari7800, {"stella",   "Stella.exe"} },
    { System::Lynx,      {"mednafen", "mednafen.exe"} },
    { System::PCE,       {"mednafen", "mednafen.exe"} },
    { System::GG,        {"mednafen", "mednafen.exe"} },
    { System::NGP,       {"mednafen", "mednafen.exe"} },
    { System::WS,        {"mednafen", "mednafen.exe"} },
    { System::VB,        {"mednafen", "mednafen.exe"} },
    { System::C64,       {"vice",     "bin/x64sc"} },
    { System::N64,       {"mupen64",  "mupen64plus"} },  // Linux: /usr/games/mupen64plus
    { System::DOS,       {"dosbox",   "dosbox"} },
    { System::NDS,       {"desmume",  "desmume-cli"} },
    { System::PS1,       {"epsxe",    "epsxe"} },
    // { System::Saturn, {"yabause",  "yabause"} },
};

static std::string exe_dir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    auto slash = path.find_last_of("/\\");
    return (slash != std::string::npos) ? path.substr(0, slash + 1) : ".\\";
#else
    char buf[PATH_MAX] = {};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return "./";
    std::string path(buf, len);
    auto slash = path.rfind('/');
    return (slash != std::string::npos) ? path.substr(0, slash + 1) : "./";
#endif
}

static bool file_exists(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

static std::string strip_slash(std::string s) {
    while (!s.empty() && (s.back() == '\\' || s.back() == '/'))
        s.pop_back();
    return s;
}

static void show_error(const std::string& title, const std::string& msg) {
#ifdef _WIN32
    MessageBoxA(nullptr, msg.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), msg.c_str(), nullptr);
#endif
}

static std::string emulator_exe(System sys) {
    // On Linux try without .exe extension first, then with
    auto it = EMULATORS.find(sys);
    if (it == EMULATORS.end()) return "";
    const auto& info = it->second;
#ifdef _WIN32
    std::string path = exe_dir() + "native\\" + info.folder + "\\" + info.exe;
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) ? path : "";
#else
    // On Linux: try bare name (no .exe), then system PATH
    std::string local = exe_dir() + "native/" + info.folder + "/" + info.exe;
    // Strip .exe for Linux
    std::string local_noext = local;
    if (local_noext.size() > 4 &&
        local_noext.substr(local_noext.size()-4) == ".exe")
        local_noext = local_noext.substr(0, local_noext.size()-4);

    if (file_exists(local_noext)) return local_noext;
    if (file_exists(local))       return local;

    // Fall back to system-installed emulator
    std::string sys_name = info.exe;
    if (sys_name.size() > 4 && sys_name.substr(sys_name.size()-4) == ".exe")
        sys_name = sys_name.substr(0, sys_name.size()-4);
    // Check if it's on PATH
    std::string which_cmd = "which " + sys_name + " > /dev/null 2>&1";
    if (system(which_cmd.c_str()) == 0) return sys_name;
    return "";
#endif
}

std::string emulator_path(System sys) { return emulator_exe(sys); }
bool emulator_available(System sys)   { return !emulator_exe(sys).empty(); }

bool launch(const Game& game) {
    std::string exe = emulator_exe(game.system);
    if (exe.empty()) {
        show_error("Emulator Not Found",
            "No emulator found for this system.\nInstall it or place it in native/");
        return false;
    }

#ifdef _WIN32
    std::string work_dir = strip_slash(exe.substr(0, exe.find_last_of("/\\")));
    std::string cmd;

    if (game.system == System::DOS) {
        if (game.dos_conf.empty() || !file_exists(game.dos_conf)) {
            show_error("DOS Launch Error",
                "DOSBox .conf not found.\nTry re-importing the game.");
            return false;
        }
        cmd = "\"" + exe + "\" -conf \"" + game.dos_conf + "\"";
    } else if (game.system == System::N64) {
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
    if (ok) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    else {
        char msg[512];
        snprintf(msg, sizeof(msg), "Launch failed (error %lu).\nCommand:\n%s",
                 GetLastError(), cmd.c_str());
        show_error("Launch Failed", msg);
    }
    return ok != 0;

#else
    // Linux: fork + exec
    std::vector<std::string> args;

    if (game.system == System::DOS) {
        if (game.dos_conf.empty()) {
            show_error("DOS Launch Error", "DOSBox .conf not found.\nTry re-importing the game.");
            return false;
        }
        args = {exe, "-conf", game.dos_conf};
    } else if (game.system == System::N64) {
        // On Linux, mupen64plus finds its plugins via system paths automatically.
        // Do NOT pass --plugindir or it will look in the wrong place.
        args = {exe, game.rom_path};
    } else {
        args = {exe, game.rom_path};
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child
        std::vector<char*> argv;
        for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(1); // exec failed
    } else if (pid < 0) {
        show_error("Launch Failed", "fork() failed.");
        return false;
    }
    return true; // parent returns immediately
#endif
}

} // namespace Launcher
