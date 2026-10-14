// The streaming music path (src/music) under the null audio backend.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "music/music.h"
#include "music/sound.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

// A stream that is already past its end: musicSoundRead (0x7D5260) answers
// every read after a song's "end" tag this way, zeroing the buffer and
// reporting one byte more than was asked for.
int PastEndOpen(char*, int) { return 1; }
int PastEndClose(int) { return 0; }
size_t PastEndRead(int, void* buf, size_t size)
{
    std::memset(buf, 0x5A, size);
    return size + 1;
}
size_t PastEndWrite(int, void*, size_t) { return 0; }
int PastEndSeek(int, long, int) { return 0; }
long PastEndTell(int) { return 0; }
long PastEndFilesize(int) { return INT_MAX; }

int CompressedQuery(char*) { return 1; }

class SoundSystem {
public:
    SoundSystem()
    {
        platform::dsound::UseNullBackend(true);
        REQUIRE(soundInit(nullptr, nullptr, 16, 0x2000, 22050) == SOUND_NO_ERROR);
    }
    ~SoundSystem() { soundClose(); }
};

} // namespace

TEST_CASE("sound: a streaming read past the end clears the buffer instead of overrunning it", "[sound]")
{
    SoundSystem system;

    // As setupNewSong allocates the song sound.
    Sound* sound = soundAllocate(SOUND_TYPE_STREAMING | SOUND_TYPE_FIRE_AND_FORGET | SOUND_TYPE_0x20, SOUND_16BIT | SOUND_CTRL_VOLUME);
    REQUIRE(sound != nullptr);
    REQUIRE(soundSetBuffers(sound, 8, 0x8000) == SOUND_NO_ERROR);
    soundSetFileIO(sound, PastEndOpen, PastEndClose, PastEndRead, PastEndWrite, PastEndSeek, PastEndTell, PastEndFilesize);

    char name[] = "empty";
    REQUIRE(soundLoad(sound, name) == SOUND_NO_ERROR);
    // soundRewind preloads again: the read that faulted in the chapter screen.
    REQUIRE(soundPlayFromPosition(sound, 0) == SOUND_NO_ERROR);
    for (int frame = 0; frame < 4; frame++) {
        soundUpdate();
    }
    soundDelete(sound);
}

#ifndef _WIN32
// The chapter screen's song start (CScreenChapter::TimerAsynchronousUpdate ->
// CSoundMixer::StartSong(n, 0x4 | 0x1) -> musicFade -> soundFadePause ->
// soundPlay -> soundRewind) after the previous song reached its "end" tag. A
// song shorter than the 8 x 0x8000 preload reaches the tag on the first read.
TEST_CASE("music: starting a song after the last one reached its end tag plays silence", "[music][data]")
{
    const char* data = std::getenv("IWD2_DATA");
    if (data == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    const fs::path acm = fs::path(data) / "Music" / "B3" / "B3b4.acm";
    std::error_code ec;
    if (!fs::exists(acm, ec)) {
        SKIP("[iwd2r-skip] no Music/B3/B3b4.acm");
    }

    // A one-section song over a short section of the install's music, ending
    // with "@TAG end" as Theme.mus does.
    const fs::path dir = fs::temp_directory_path() / ("iwd2r-music-" + std::to_string(::getpid()));
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "B3");
    fs::copy_file(acm, dir / "B3" / "B3b4.acm");
    {
        std::ofstream mus(dir / "B3.mus");
        mus << "B3\n1\nb4   @TAG end\n";
    }

    {
        SoundSystem system;
        REQUIRE(musicInit(CompressedQuery) == 0);
        musicSetPath(dir.string().c_str(), "acm");
        std::string song = (dir / "B3.mus").string();
        char* songs[] = { song.data() };
        REQUIRE(musicLoadSongList(songs, 1) == 0);
        musicSetVolume(VOLUME_MAX);

        // internalMusicPlay: soundLoad's preload reads the whole section and
        // passes the end tag, then soundPlayFromPosition preloads again.
        REQUIRE(musicForceSection(0, 0, 0) == 0);
        CHECK(musicGetSection() == -2);
        // StartSong's flag 5: a 200 ms fade into the next song.
        musicFade(0, 0, -1, 200);
        for (int frame = 0; frame < 4; frame++) {
            soundUpdate();
        }
        musicForceStop();
        musicClose();
    }
    fs::remove_all(dir, ec);
}
#endif
