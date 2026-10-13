#define NOMINMAX
#include "platform/script.h"

#include "platform/data_dir.h"
#include "platform/png.h"
#include "platform/present.h"
#include "platform/replay.h"
#include "platform/window.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>

namespace {

using platform::script::Command;

[[noreturn]] void Fail(const std::string& text)
{
    platform::ReportFatal(text.c_str(), "iwd2r script");
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(5);
}

// Writes the primary surface as <IWD2R_SHOT_DIR>/<name>.png. On failure
// returns false with why set.
bool WriteShot(const std::string& name, std::string& path, std::string& why)
{
    std::string dir;
    if (const char* env = std::getenv("IWD2R_SHOT_DIR"))
        dir = env;
    path = dir;
    if (!path.empty() && path.back() != '/' && path.back() != '\\')
        path += path.find('\\') != std::string::npos ? '\\' : '/';
    path += name + ".png";

    const uint32_t* pixels = nullptr;
    int w = 0;
    int h = 0;
    int pitch = 0;
    if (!platform::present::PrimaryFrame(&pixels, &w, &h, &pitch)) {
        why = "no primary surface yet";
        return false;
    }
    if (!platform::WritePng(path.c_str(), pixels, w, h, pitch)) {
        why = "cannot write " + path;
        return false;
    }
    why = std::to_string(w) + "x" + std::to_string(h);
    return true;
}

class EngineEffects : public platform::script::Effects {
public:
    void Shot(const std::string& name, int line) override
    {
        std::string path;
        std::string why;
        if (!WriteShot(name, path, why))
            Fail("line " + std::to_string(line) + ": shot " + name + ": " + why);
        std::printf("script: line %d: shot %s (%s)\n", line, path.c_str(), why.c_str());
        std::fflush(stdout);
    }

    void Quit(int) override
    {
        platform::window::EnqueueMessage(WM_CLOSE, 0, 0);
    }

    // The replay mode writes the dump; otherwise nothing to do.
    void Checkpoint(const std::string& name, int line) override
    {
        if (platform::replay::Active())
            platform::replay::Checkpoint(name, line);
    }
};

struct Runtime {
    std::mutex mutex;
    platform::script::Player player;
    // Replay mode: the parsed script, until frame 1 starts it.
    std::vector<Command> pending;
    bool hasPending = false;
};

Runtime& R()
{
    static Runtime runtime;
    return runtime;
}

}

namespace platform::script {

void Start(std::vector<Command> commands)
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    const SIZE screen = platform::present::ScreenSize();
    r.player.Start(commands, screen.cx, screen.cy);
}

void StartFromEnvironment()
{
    const char* path = std::getenv("IWD2R_SCRIPT");
    if (!path || !*path)
        return;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        Fail(std::string(path) + ": cannot read the script");
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<Command> commands;
    std::string error;
    if (!Parse(text, commands, error))
        Fail(std::string(path) + ": " + error);
    std::printf("script: %s (%zu commands)\n", path, commands.size());
    std::fflush(stdout);
    if (platform::replay::Active()) {
        Runtime& r = R();
        std::lock_guard<std::mutex> lock(r.mutex);
        r.pending = std::move(commands);
        r.hasPending = true;
        return;
    }
    Start(std::move(commands));
}

bool StartPending(int width, int height)
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    if (!r.hasPending)
        return false;
    r.hasPending = false;
    r.player.Start(r.pending, width, height);
    return true;
}

void Stop()
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.player.Stop();
}

bool Advance()
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    EngineEffects effects;
    if (r.player.Advance(effects)) {
        std::printf("script: finished\n");
        std::fflush(stdout);
        return true;
    }
    return false;
}

int Line()
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    return r.player.Line();
}

bool Running()
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    return r.player.Running();
}

bool InjectedCursor(POINT* point)
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    return r.player.InjectedCursor(point);
}

bool InjectedKey(int vk, SHORT* state)
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    return r.player.InjectedKey(vk, state);
}

void PollControl()
{
    const char* dir = std::getenv("IWD2R_CONTROL_DIR");
    if (!dir || !*dir)
        return;
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<fs::path> requests;
    bool framesRequested = false;
    for (fs::directory_iterator it(fs::path(dir), ec), end; !ec && it != end; it.increment(ec)) {
        const std::string file = it->path().filename().string();
        if (file == "frames.req")
            framesRequested = true;
        else if (file.size() > 8 && file.compare(0, 5, "shot-") == 0 && file.compare(file.size() - 4, 4, ".req") == 0)
            requests.push_back(it->path());
    }
    if (framesRequested) {
        // Written aside and renamed, so a reader never sees a partial file.
        // rename replaces frames.txt in one step; deleting it first would
        // leave it pending deletion on Windows, where opening a file in that
        // state fails with "Access is denied".
        const fs::path answer = fs::path(dir) / "frames.txt";
        const fs::path temp = fs::path(dir) / "frames.tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << platform::present::PresentedFrames() << "\n";
        }
        fs::rename(temp, answer, ec);
        fs::remove(fs::path(dir) / "frames.req", ec);
    }
    for (const fs::path& request : requests) {
        const std::string file = request.filename().string();
        const std::string name = file.substr(5, file.size() - 9);
        std::string path;
        std::string why;
        if (WriteShot(name, path, why)) {
            std::printf("control: shot %s (%s)\n", path.c_str(), why.c_str());
        } else if (why == "no primary surface yet") {
            continue; // retried on the next frame
        } else {
            std::printf("control: shot %s failed: %s\n", name.c_str(), why.c_str());
        }
        std::fflush(stdout);
        fs::remove(request, ec);
    }
}

}
