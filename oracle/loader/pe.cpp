// iwd2r. Distributed under LICENSE.md.
#include "pe.h"

#include <cstdio>
#include <utility>

namespace {

constexpr uint16_t kMachineI386 = 0x14C;
constexpr uint16_t kMagicPe32 = 0x10B;
constexpr uint16_t kMagicPe32Plus = 0x20B;
constexpr uint32_t kImportDirectory = 1;
constexpr uint32_t kOrdinalFlag = 0x80000000u;
constexpr size_t kMaxImports = 1 << 16; // guards against looping thunk tables

std::string Hex(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%X", value);
    return text;
}

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& file)
        : file_(file)
    {
    }

    bool Has(uint64_t offset, uint64_t size) const { return offset <= file_.size() && size <= file_.size() - offset; }

    uint16_t U16(uint32_t offset) const { return static_cast<uint16_t>(file_[offset] | file_[offset + 1] << 8); }

    uint32_t U32(uint32_t offset) const
    {
        return static_cast<uint32_t>(file_[offset]) | static_cast<uint32_t>(file_[offset + 1]) << 8
            | static_cast<uint32_t>(file_[offset + 2]) << 16 | static_cast<uint32_t>(file_[offset + 3]) << 24;
    }

    // A NUL-terminated string at offset, or false if it runs past the end.
    bool String(uint32_t offset, std::string* out) const
    {
        out->clear();
        for (size_t i = offset; i < file_.size(); ++i) {
            if (file_[i] == 0) {
                return true;
            }
            out->push_back(static_cast<char>(file_[i]));
        }
        return false;
    }

private:
    const std::vector<uint8_t>& file_;
};

// The file offset of rva, or false when no section's raw data holds it.
bool RvaToOffset(const PeImage& image, uint32_t rva, uint32_t* offset)
{
    if (rva < image.headersSize) {
        *offset = rva;
        return true;
    }
    for (const PeSection& s : image.sections) {
        if (rva >= s.rva && rva - s.rva < s.rawSize) {
            *offset = s.rawOffset + (rva - s.rva);
            return true;
        }
    }
    return false;
}

bool Fail(std::string* error, const std::string& reason)
{
    if (error) {
        *error = reason;
    }
    return false;
}

bool ParseImports(const Reader& r, PeImage* image, uint32_t dirRva, std::string* error)
{
    for (uint32_t descRva = dirRva;; descRva += 20) {
        uint32_t desc;
        if (!RvaToOffset(*image, descRva, &desc) || !r.Has(desc, 20)) {
            return Fail(error, "import descriptor at RVA " + Hex(descRva) + " is outside the file");
        }
        const uint32_t lookupRva = r.U32(desc);
        const uint32_t nameRva = r.U32(desc + 12);
        const uint32_t iatRva = r.U32(desc + 16);
        if (lookupRva == 0 && nameRva == 0 && iatRva == 0) {
            return true;
        }
        uint32_t nameOffset;
        std::string dll;
        if (!RvaToOffset(*image, nameRva, &nameOffset) || !r.String(nameOffset, &dll)) {
            return Fail(error, "import DLL name at RVA " + Hex(nameRva) + " is outside the file");
        }
        const uint32_t thunksRva = lookupRva != 0 ? lookupRva : iatRva;
        for (uint32_t i = 0;; ++i) {
            if (image->imports.size() >= kMaxImports) {
                return Fail(error, "import table of " + dll + " does not terminate");
            }
            uint32_t thunk;
            if (!RvaToOffset(*image, thunksRva + 4 * i, &thunk) || !r.Has(thunk, 4)) {
                return Fail(error, "import thunks of " + dll + " run outside the file");
            }
            const uint32_t value = r.U32(thunk);
            if (value == 0) {
                break;
            }
            PeImport import;
            import.dll = dll;
            import.iatRva = iatRva + 4 * i;
            if (value & kOrdinalFlag) {
                import.ordinal = static_cast<uint16_t>(value & 0xFFFF);
            } else {
                uint32_t hint;
                if (!RvaToOffset(*image, value, &hint) || !r.Has(hint, 2) || !r.String(hint + 2, &import.name)) {
                    return Fail(error, "import name at RVA " + Hex(value) + " of " + dll + " is outside the file");
                }
                import.ordinal = r.U16(hint);
            }
            image->imports.push_back(import);
        }
    }
}

} // namespace

bool ParsePe(const std::vector<uint8_t>& file, PeImage* out, std::string* error)
{
    const Reader r(file);
    if (!r.Has(0, 0x40)) {
        return Fail(error, "file is too short for a DOS header");
    }
    if (file[0] != 'M' || file[1] != 'Z') {
        return Fail(error, "no MZ signature");
    }
    const uint32_t pe = r.U32(0x3C);
    if (!r.Has(pe, 24)) {
        return Fail(error, "PE header at " + Hex(pe) + " is past the end of the file");
    }
    if (r.U32(pe) != 0x00004550) {
        return Fail(error, "no PE signature at " + Hex(pe));
    }
    const uint16_t machine = r.U16(pe + 4);
    const uint16_t sectionCount = r.U16(pe + 6);
    const uint16_t optionalSize = r.U16(pe + 20);
    const uint32_t opt = pe + 24;
    if (!r.Has(opt, 2)) {
        return Fail(error, "optional header is past the end of the file");
    }
    const uint16_t magic = r.U16(opt);
    if (magic == kMagicPe32Plus) {
        return Fail(error, "PE32+ (64-bit) image; only PE32 is supported");
    }
    if (magic != kMagicPe32) {
        return Fail(error, "unknown optional header magic " + Hex(magic));
    }
    if (machine != kMachineI386) {
        return Fail(error, "machine " + Hex(machine) + " is not i386");
    }
    if (optionalSize < 96 || !r.Has(opt, optionalSize)) {
        return Fail(error, "optional header is truncated");
    }
    PeImage image;
    image.entryRva = r.U32(opt + 16);
    image.imageBase = r.U32(opt + 28);
    image.imageSize = r.U32(opt + 56);
    image.headersSize = r.U32(opt + 60);
    const uint32_t directoryCount = r.U32(opt + 92);
    uint32_t importRva = 0;
    if (directoryCount > kImportDirectory && optionalSize >= 96 + 8 * (kImportDirectory + 1)) {
        importRva = r.U32(opt + 96 + 8 * kImportDirectory);
    }
    const uint32_t table = opt + optionalSize;
    if (!r.Has(table, 40ull * sectionCount)) {
        return Fail(error, "section table is past the end of the file");
    }
    for (uint32_t i = 0; i < sectionCount; ++i) {
        const uint32_t s = table + 40 * i;
        PeSection section;
        for (uint32_t c = 0; c < 8 && file[s + c] != 0; ++c) {
            section.name.push_back(static_cast<char>(file[s + c]));
        }
        section.virtualSize = r.U32(s + 8);
        section.rva = r.U32(s + 12);
        section.rawSize = r.U32(s + 16);
        section.rawOffset = r.U32(s + 20);
        section.characteristics = r.U32(s + 36);
        if (section.rawSize != 0 && !r.Has(section.rawOffset, section.rawSize)) {
            return Fail(error, "section " + section.name + " points past the end of the file");
        }
        const uint32_t mapped = section.virtualSize != 0 ? section.virtualSize : section.rawSize;
        if (uint64_t(section.rva) + mapped > image.imageSize) {
            return Fail(error, "section " + section.name + " extends past SizeOfImage");
        }
        image.sections.push_back(section);
    }
    if (importRva != 0 && !ParseImports(r, &image, importRva, error)) {
        return false;
    }
    *out = std::move(image);
    return true;
}
