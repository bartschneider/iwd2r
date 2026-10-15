#define NOMINMAX
#include "platform/text_layer.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace platform::text {

namespace {

    // A glyph not shown for this many presents is forgotten once the layer
    // holds more than kSoftLimit glyphs (about half a minute at 60 frames a
    // second: longer than the pointer rests over one character, whose
    // save-under brings the tags back when it moves).
    const uint32_t kForgetAfter = 1800;
    const size_t kSoftLimit = 4096;
    // Above this, the glyphs not shown in the last present go too.
    const size_t kHardLimit = 32768;

    bool Empty(const RECT& r)
    {
        return r.right <= r.left || r.bottom <= r.top;
    }

    bool Overlap(const RECT& a, const RECT& b)
    {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }

    bool Contains(const RECT& outer, const RECT& inner)
    {
        return inner.left >= outer.left && inner.right <= outer.right && inner.top >= outer.top && inner.bottom <= outer.bottom;
    }

    struct Settings {
        std::mutex mutex;
        bool enabled = false;
        float fontScale = 1.0f;
        std::function<bool()> toggle;
    };

    Settings& S()
    {
        static Settings settings;
        return settings;
    }

    std::string Trimmed(const char* text)
    {
        std::string s = text ? text : "";
        const size_t first = s.find_first_not_of(" \t");
        const size_t last = s.find_last_not_of(" \t");
        s = first == std::string::npos ? std::string() : s.substr(first, last - first + 1);
        for (char& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

}

bool IsTag(uint8_t value)
{
    return value != 0x00 && value != 0xFF && value != 0x7F && value != 0x80;
}

uint8_t Layer::Add(const Glyph& g, std::vector<Glyph>* kept)
{
    if (kept)
        kept->clear();
    if (Empty(g.box))
        return 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    bool used[256] = {};
    for (auto it = m_glyphs.begin(); it != m_glyphs.end();) {
        const Glyph& e = it->second;
        if (!Overlap(e.box, g.box)) {
            ++it;
            continue;
        }
        if (Contains(g.box, e.box)) {
            it = m_glyphs.erase(it);
            continue;
        }
        used[e.tag] = true;
        if (kept)
            kept->push_back(e);
        ++it;
    }
    int tag = 1;
    while (tag < 255 && (!IsTag(static_cast<uint8_t>(tag)) || used[tag]))
        tag++;
    if (tag >= 255) {
        // More than 250 live glyphs overlap this one: impossible for text
        // drawn line by line; reuse the first tag rather than drop it.
        tag = 1;
    }
    Glyph added = g;
    added.tag = static_cast<uint8_t>(tag);
    added.serial = ++m_serial;
    added.seen = m_presents;
    m_glyphs[added.serial] = added;
    return added.tag;
}

void Layer::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_glyphs.clear();
}

size_t Layer::Size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_glyphs.size();
}

std::vector<Glyph> Layer::Glyphs() const
{
    std::vector<Glyph> out;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        out.reserve(m_glyphs.size());
        for (const auto& entry : m_glyphs)
            out.push_back(entry.second);
    }
    std::sort(out.begin(), out.end(), [](const Glyph& a, const Glyph& b) { return a.serial < b.serial; });
    return out;
}

void Layer::Presented(const std::vector<uint32_t>& visibleSerials)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_presents++;
    for (uint32_t serial : visibleSerials) {
        auto it = m_glyphs.find(serial);
        if (it != m_glyphs.end())
            it->second.seen = m_presents;
    }
    if (m_glyphs.size() <= kSoftLimit)
        return;
    const bool hard = m_glyphs.size() > kHardLimit;
    for (auto it = m_glyphs.begin(); it != m_glyphs.end();) {
        const uint32_t age = m_presents - it->second.seen;
        if (age > kForgetAfter || (hard && age > 0))
            it = m_glyphs.erase(it);
        else
            ++it;
    }
}

uint32_t Layer::PresentCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_presents;
}

Layer& BackLayer()
{
    static Layer layer;
    return layer;
}

void SetEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(S().mutex);
    S().enabled = enabled;
}

bool Enabled()
{
    std::lock_guard<std::mutex> lock(S().mutex);
    return S().enabled;
}

void SetFontScale(float scale)
{
    std::lock_guard<std::mutex> lock(S().mutex);
    S().fontScale = std::min(2.0f, std::max(0.5f, scale));
}

float FontScale()
{
    std::lock_guard<std::mutex> lock(S().mutex);
    return S().fontScale;
}

bool ParseFontScale(const char* text, float* scale)
{
    const std::string s = Trimmed(text);
    if (s.empty())
        return false;
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end != s.c_str() + s.size() || !(v >= 0.5f && v <= 2.0f))
        return false;
    *scale = v;
    return true;
}

bool ParseRenderer(const char* text, bool* scalable)
{
    const std::string s = Trimmed(text);
    if (s == "classic")
        *scalable = false;
    else if (s == "scalable")
        *scalable = true;
    else
        return false;
    return true;
}

void SetToggleHandler(std::function<bool()> handler)
{
    std::lock_guard<std::mutex> lock(S().mutex);
    S().toggle = std::move(handler);
}

bool Toggle()
{
    std::function<bool()> handler;
    {
        std::lock_guard<std::mutex> lock(S().mutex);
        handler = S().toggle;
    }
    return handler ? handler() : false;
}

}
