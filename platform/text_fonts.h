// Scalable text: which TrueType face draws each bitmap font, and the
// character set. The faces are SIL Open Font License fonts bundled in
// third_party/fonts/ and embedded into the executable at configure time
// (platform/CMakeLists.txt), listed in docs/THIRD_PARTY.md.
#pragma once

#include <cstddef>
#include <cstdint>

namespace platform::text {

struct Face {
    const char* name; // the font file, e.g. "Cinzel-Regular.ttf"
    const unsigned char* data; // the embedded file
    size_t size;
};

// The face for a bitmap font (its resref, any case, e.g. "REALMS") and the
// size factor on its cap height; false for a font that keeps the bitmap
// glyphs (drop caps, symbol fonts, anything not in the table).
bool FaceFor(const char* resref, int* face, float* sizeFactor);

// A face by number (0 .. FaceCount() - 1); null for any other number or a
// face whose file is not embedded.
const Face* FaceAt(int face);
int FaceCount();

// A Windows-1252 byte (dialog.tlk's encoding) as a Unicode code point. The
// five unassigned bytes (0x81, 0x8D, 0x8F, 0x90, 0x9D) map to themselves.
uint16_t DecodeCp1252(uint8_t byte);

}
