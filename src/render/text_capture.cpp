#include "render/text_capture.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "CChitin.h"
#include "CResCell.h"
#include "CVidFont.h"
#include "CVidInf.h"
#include "CVidPalette.h"
#include "platform/text_fonts.h"
#include "platform/text_layer.h"

namespace render::text_capture {

namespace {

    // What the capture knows of one bitmap font, read once from its BAM.
    struct FontInfo {
        bool mapped = false;
        int face = -1;
        float sizeFactor = 1.0f;
        // 'H': the rows its ink spans (cap height) and where its baseline
        // lies relative to the y TextOut is given (the row under its ink).
        float capHeight = 0.0f;
        int baselineOffset = 0;
        // Each byte's frame (sequence byte - 1, frame 0, as TextOut draws
        // it) as an ink mask at the resource's size.
        struct Frame {
            int w = 0;
            int h = 0;
            std::vector<uint8_t> ink;
            bool any = false;
        };
        Frame frames[256];
        // How often each palette index is ink in the letters and digits.
        std::vector<std::pair<int, int>> inkIndices;
    };

    struct State {
        Backend backend;
        bool installed = false;
        bool fxNoted = false;
        unsigned long captured = 0;
        std::unordered_map<std::string, FontInfo> fonts;
        std::set<std::string> debugSeen;
    };

    State& S()
    {
        static State state;
        return state;
    }

    bool Debug()
    {
        static const bool debug = [] {
            const char* v = std::getenv("IWD2R_TEXT_DEBUG");
            return v != nullptr && *v != '\0' && std::strcmp(v, "0") != 0;
        }();
        return debug;
    }

    std::string Upper(const CString& s)
    {
        std::string out(static_cast<const char*>(s));
        for (char& c : out)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return out;
    }

    float Luminance(uint32_t rgb)
    {
        const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
        const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
        const float b = static_cast<float>(rgb & 0xFF) / 255.0f;
        return 0.299f * r + 0.587f * g + 0.114f * b;
    }

    // A frame's pixels, RLE-decoded (BAM V1: the transparent index followed
    // by a count of further transparent pixels).
    std::vector<uint8_t> Decode(CResCell& res, FRAMEENTRY* frame, uint8_t transparent)
    {
        const int w = frame->nWidth;
        const int h = frame->nHeight;
        std::vector<uint8_t> out(static_cast<size_t>(w) * static_cast<size_t>(h), transparent);
        const BYTE* data = res.GetFrameData(frame, FALSE);
        if (data == nullptr || w <= 0 || h <= 0)
            return out;
        if (!res.GetCompressed(frame, FALSE)) {
            std::memcpy(out.data(), data, out.size());
            return out;
        }
        size_t pos = 0;
        size_t at = 0;
        while (at < out.size()) {
            const uint8_t value = data[pos++];
            if (value == transparent) {
                const size_t run = static_cast<size_t>(data[pos++]) + 1;
                at += run;
            } else {
                out[at++] = value;
            }
        }
        return out;
    }

    void Analyze(CVidFont& font, FontInfo& info)
    {
        const std::string resref = Upper(font.GetResRef().GetResRefStr());
        info.mapped = platform::text::FaceFor(resref.c_str(), &info.face, &info.sizeFactor);
        CResCell* res = font.pRes;
        if (!info.mapped || res == nullptr || res->Demand() == nullptr) {
            info.mapped = false;
            return;
        }
        BAMHEADER* header = res->m_bCacheHeader ? res->m_pBamHeaderCopy : res->m_pBamHeader;
        if (header == nullptr || res->m_pSequences == nullptr) {
            res->Release();
            info.mapped = false;
            return;
        }
        const uint8_t transparent = header->nTransparentColor;
        int counts[256] = {};
        for (int byte = 1; byte < 256; byte++) {
            int sequence = byte - 1;
            if (sequence >= header->nSequences)
                sequence = 0;
            if (res->m_pSequences[sequence].nFrames <= 0)
                continue;
            FRAMEENTRY* frame = res->GetFrame(static_cast<WORD>(sequence), 0, FALSE);
            if (frame == nullptr)
                continue;
            FontInfo::Frame& f = info.frames[byte];
            f.w = frame->nWidth;
            f.h = frame->nHeight;
            const std::vector<uint8_t> pixels = Decode(*res, frame, transparent);
            f.ink.assign(pixels.size(), 0);
            const bool counted = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
            for (size_t i = 0; i < pixels.size(); i++) {
                if (pixels[i] == transparent || pixels[i] == 0)
                    continue;
                f.ink[i] = 1;
                f.any = true;
                if (counted)
                    counts[pixels[i]]++;
            }
            if (byte == 'H') {
                int top = -1;
                int bottom = -1;
                for (int y = 0; y < f.h; y++) {
                    for (int x = 0; x < f.w; x++) {
                        if (f.ink[static_cast<size_t>(y) * static_cast<size_t>(f.w) + static_cast<size_t>(x)]) {
                            if (top < 0)
                                top = y;
                            bottom = y;
                        }
                    }
                }
                if (top >= 0) {
                    info.capHeight = static_cast<float>(bottom - top + 1);
                    info.baselineOffset = bottom + 1 - frame->nCenterY;
                }
            }
        }
        res->Release();
        for (int index = 0; index < 256; index++) {
            if (counts[index] > 0)
                info.inkIndices.push_back({ index, counts[index] });
        }
        std::sort(info.inkIndices.begin(), info.inkIndices.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (info.capHeight <= 0.0f || info.inkIndices.empty())
            info.mapped = false;
    }

    FontInfo& Info(CVidFont& font)
    {
        const std::string resref = Upper(font.GetResRef().GetResRefStr());
        auto it = S().fonts.find(resref);
        if (it != S().fonts.end())
            return it->second;
        FontInfo& info = S().fonts[resref];
        Analyze(font, info);
        return info;
    }

    // The colours the bitmap path draws the string in: the font's palette
    // realized the way CVidFont::TextOut (0x793820) realizes it, into the
    // same scratch palette. The body is the most common ink index's colour;
    // light text gets the darkest common ink colour as its outline.
    void Colours(CVidFont& font, const FontInfo& info, DWORD flags, uint32_t* body, uint32_t* outline, bool* outlined)
    {
        if (!font.m_bPaletteChanged)
            font.m_cPalette.SetPalette(font.pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
        if (flags != 0)
            font.m_cPalette.Realize(CVidImage::rgbTempPal, g_pChitin->cVideo.m_nBpp, flags, &font.m_paletteAffects, 255);
        else
            font.m_cPalette.Realize(CVidImage::rgbTempPal, g_pChitin->cVideo.m_nBpp, 0, NULL, 255);
        *body = CVidImage::rgbTempPal[info.inkIndices.front().first] & 0xFFFFFFu;
        int total = 0;
        for (const auto& entry : info.inkIndices)
            total += entry.second;
        uint32_t darkest = 0;
        float darkestLum = 2.0f;
        for (const auto& entry : info.inkIndices) {
            if (entry.second * 20 < total)
                continue;
            const uint32_t c = CVidImage::rgbTempPal[entry.first] & 0xFFFFFFu;
            const float l = Luminance(c);
            if (l < darkestLum) {
                darkestLum = l;
                darkest = c;
            }
        }
        *outlined = Luminance(*body) >= 0.35f;
        *outline = darkestLum < 0.3f ? darkest : 0u;
    }

    RECT Intersect(const RECT& a, const RECT& b)
    {
        RECT r;
        r.left = a.left > b.left ? a.left : b.left;
        r.top = a.top > b.top ? a.top : b.top;
        r.right = a.right < b.right ? a.right : b.right;
        r.bottom = a.bottom < b.bottom ? a.bottom : b.bottom;
        return r;
    }

    bool Inside(const RECT& r, LONG x, LONG y)
    {
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }

}

std::vector<Cell> Cells(CVidFont& font, const char* text, POINT pos)
{
    std::vector<Cell> cells;
    int x = pos.x;
    for (const char* p = text; *p != '\0'; p++) {
        const char ch = *p;
        const SHORT nSequence = ch < 0 ? ch + 256 : ch;
        font.SequenceSet(nSequence - 1);
        font.FrameSet(0);
        CSize size;
        font.GetCurrentFrameSize(size, FALSE);
        CPoint ptCenter;
        font.GetCurrentCenterPoint(ptCenter, FALSE);
        Cell cell;
        cell.byte = static_cast<unsigned char>(ch);
        cell.rect = RECT { x, pos.y - ptCenter.y, x + size.cx, pos.y + size.cy - ptCenter.y };
        cells.push_back(cell);
        x += size.cx;
    }
    return cells;
}

bool Backend::DrawText(const CVidFont& constFont, const char* text, POINT pos, DWORD flags)
{
    if (g_pChitin == nullptr || text == nullptr)
        return false;
    // The original's own TrueType path and the inert 3d path draw as before.
    if (g_pChitin->cVideo.m_bIs3dAccelerated || g_pChitin->m_sFontName.Compare("") != 0 || g_pChitin->field_1A0)
        return false;
    CVidFont& font = const_cast<CVidFont&>(constFont);
    if (font.pRes == nullptr)
        return false;
    FontInfo& info = Info(font);
    if (!info.mapped)
        return false;

    CVidMode* mode = g_pChitin->GetCurrentVideoMode();
    if (mode == nullptr || mode->pSurfaces == nullptr || mode->pSurfaces[CVIDINF_SURFACE_BACK] == nullptr)
        return false;
    IDirectDrawSurface* back = mode->pSurfaces[CVIDINF_SURFACE_BACK];
    DDSURFACEDESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    if (back->Lock(NULL, &desc, DDLOCK_WAIT, NULL) != DD_OK)
        return false;
    struct Unlock {
        IDirectDrawSurface* surface;
        void* pixels;
        ~Unlock() { surface->Unlock(pixels); }
    } unlock { back, desc.lpSurface };
    if (desc.ddpfPixelFormat.dwRGBBitCount != 32 || desc.lpSurface == nullptr)
        return false;
    const BYTE* begin = static_cast<const BYTE*>(desc.lpSurface);
    const BYTE* end = begin + static_cast<size_t>(desc.lPitch) * desc.dwHeight;
    // BKTextOut draws into the back surface locked at m_rLockedRect;
    // FXTextOut into the FX surface, which FXBltToBack composites later at
    // a place this call does not know: that text keeps the bitmap glyphs.
    // CVidFont::TextOut(..., nSurface) locks nothing itself (its only
    // caller, CGameSprite's hit points, passes CVIDINF_SURFACE_BACK).
    const CVidInf* inf = static_cast<const CVidInf*>(mode);
    const BYTE* locked = static_cast<const BYTE*>(inf->m_SurfaceDesc.lpSurface);
    if (locked != nullptr && (locked < begin || locked >= end)) {
        if (!S().fxNoted) {
            S().fxNoted = true;
            std::fprintf(stderr, "text: FX-surface text (CVidInf::FXTextOut: the tooltips, CInfToolTip::RenderText) keeps the bitmap font\n");
        }
        return false;
    }

    // Lay the string out as the body would (its side effects included).
    const std::vector<Cell> cells = Cells(font, text, pos);

    uint32_t body = 0;
    uint32_t outline = 0;
    bool outlined = false;
    if (font.pRes->Demand() == nullptr)
        return false;
    Colours(font, info, flags, &body, &outline, &outlined);
    font.pRes->Release();

    const int scale = font.m_bDoubleSize ? 2 : 1;
    const RECT surface { 0, 0, static_cast<LONG>(desc.dwWidth), static_cast<LONG>(desc.dwHeight) };
    uint32_t* pixels = static_cast<uint32_t*>(desc.lpSurface);
    const size_t pitch = static_cast<size_t>(desc.lPitch) / 4;
    std::vector<platform::text::Glyph> kept;
    for (const Cell& cell : cells) {
        const FontInfo::Frame& frame = info.frames[cell.byte];
        if (!frame.any)
            continue;
        platform::text::Glyph g;
        g.cell = cell.rect;
        g.clip = Intersect(m_clip, surface);
        g.box = Intersect(cell.rect, g.clip);
        if (g.box.right <= g.box.left || g.box.bottom <= g.box.top)
            continue;
        g.baseline = pos.y + info.baselineOffset * scale;
        g.capHeight = info.capHeight * static_cast<float>(scale) * info.sizeFactor;
        g.colour = body;
        g.outline = outline;
        g.codepoint = platform::text::DecodeCp1252(cell.byte);
        g.face = static_cast<uint8_t>(info.face);
        g.flags = outlined ? platform::text::kOutline : 0;
        const uint8_t tag = platform::text::BackLayer().Add(g, &kept);
        if (tag == 0)
            continue;
        S().captured++;
        // Tag the box: the glyph's own ink always (it covers what was
        // there), the rest of its cell unless another live glyph still
        // owns that pixel (a bitmap glyph's transparent pixels leave it).
        for (LONG y = g.box.top; y < g.box.bottom; y++) {
            uint32_t* row = pixels + static_cast<size_t>(y) * pitch;
            const int fy = static_cast<int>(y - cell.rect.top) / scale;
            for (LONG x = g.box.left; x < g.box.right; x++) {
                const int fx = static_cast<int>(x - cell.rect.left) / scale;
                const bool ink = fy < frame.h && fx < frame.w
                    && frame.ink[static_cast<size_t>(fy) * static_cast<size_t>(frame.w) + static_cast<size_t>(fx)] != 0;
                uint32_t& p = row[x];
                if (!ink) {
                    const uint8_t owner = platform::text::TagOf(p);
                    bool other = false;
                    if (platform::text::IsTag(owner)) {
                        for (const platform::text::Glyph& k : kept) {
                            if (k.tag == owner && Inside(k.box, x, y)) {
                                other = true;
                                break;
                            }
                        }
                    }
                    if (other)
                        continue;
                }
                p = (p & 0x00FFFFFFu) | (static_cast<uint32_t>(tag) << 24);
            }
        }
    }

    if (Debug()) {
        char key[160];
        std::snprintf(key, sizeof(key), "%s %06X/%06X %u %d", Upper(font.GetResRef().GetResRefStr()).c_str(),
            static_cast<unsigned>(font.m_rgbForegroundColor), static_cast<unsigned>(font.m_rgbBackgroundColor),
            static_cast<unsigned>(flags), font.m_bPaletteChanged ? 1 : 0);
        if (S().debugSeen.insert(key).second) {
            std::fprintf(stderr, "text: %s body %06X outline %06X%s cap %.1f base %+d \"%s\"\n", key,
                static_cast<unsigned>(body), static_cast<unsigned>(outline), outlined ? "" : " (none)",
                static_cast<double>(info.capHeight), info.baselineOffset, text);
        }
    }
    return true;
}

void Install()
{
    State& s = S();
    platform::text::BackLayer().Clear();
    if (!s.installed) {
        render::Set(&s.backend);
        s.installed = true;
    }
    platform::text::SetEnabled(true);
}

void Uninstall()
{
    State& s = S();
    if (s.installed) {
        if (render::Active() == &s.backend)
            render::Set(nullptr);
        s.installed = false;
    }
    platform::text::SetEnabled(false);
    platform::text::BackLayer().Clear();
}

bool Installed()
{
    return S().installed;
}

unsigned long CapturedGlyphs()
{
    return S().captured;
}

bool Toggle()
{
    if (Installed())
        Uninstall();
    else
        Install();
    if (g_pChitin != nullptr)
        g_pChitin->RedrawScreen();
    return Installed();
}

}
