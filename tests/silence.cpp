// Keeps every engine the tests start silent: the engine's audio is miniaudio,
// which ignores SDL_AUDIO_DRIVER, and on WSL with WSLg real audio reaches the
// Windows speakers. IWD2R_AUDIO=null selects the null backend; child engines
// the smoke tests spawn inherit it.
#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <cstdlib>

namespace {

void SetIfUnset(const char* name, const char* value)
{
    if (std::getenv(name) != nullptr)
        return;
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 0);
#endif
}

class SilenceListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const&) override
    {
        SetIfUnset("IWD2R_AUDIO", "null");
        SetIfUnset("SDL_AUDIO_DRIVER", "dummy");
    }
};

} // namespace

CATCH_REGISTER_LISTENER(SilenceListener)
