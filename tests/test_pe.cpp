// iwd2r. Distributed under LICENSE.md.
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>

#include "oracle/loader/pe.h"

// IWD2R_TEST_PE is the file tests/make_test_pe.py writes at build time.
namespace {

std::vector<uint8_t> TestPe()
{
    std::ifstream in(IWD2R_TEST_PE, std::ios::binary);
    REQUIRE(in);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void Put32(std::vector<uint8_t>* file, size_t offset, uint32_t value)
{
    for (int i = 0; i < 4; ++i) {
        (*file)[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

std::string ParseError(const std::vector<uint8_t>& file)
{
    PeImage image;
    std::string error;
    REQUIRE_FALSE(ParsePe(file, &image, &error));
    return error;
}

// Layout constants of the generated file (see make_test_pe.py).
constexpr size_t kOptionalHeader = 0x40 + 24;
constexpr size_t kSectionTable = kOptionalHeader + 0xE0;

} // namespace

TEST_CASE("ParsePe reads the base, sections and imports of a PE32", "[pe]")
{
    PeImage image;
    std::string error;
    REQUIRE(ParsePe(TestPe(), &image, &error));
    CHECK(error.empty());
    CHECK(image.imageBase == 0x10000000u);
    CHECK(image.imageSize == 0x3000u);
    CHECK(image.entryRva == 0x1000u);
    CHECK(image.headersSize == 0x200u);

    REQUIRE(image.sections.size() == 2);
    CHECK(image.sections[0].name == ".text");
    CHECK(image.sections[0].rva == 0x1000u);
    CHECK(image.sections[0].virtualSize == 0x10u);
    CHECK(image.sections[0].rawOffset == 0x200u);
    CHECK(image.sections[0].rawSize == 0x200u);
    CHECK(image.sections[0].characteristics == 0x60000020u);
    CHECK(image.sections[1].name == ".idata");
    CHECK(image.sections[1].rva == 0x2000u);
    CHECK(image.sections[1].virtualSize == 0x100u);
    CHECK(image.sections[1].rawOffset == 0x400u);
    CHECK(image.sections[1].rawSize == 0x200u);
    CHECK(image.sections[1].characteristics == 0xC0000040u);

    REQUIRE(image.imports.size() == 2);
    CHECK(image.imports[0].dll == "KERNEL32.dll");
    CHECK(image.imports[0].name == "GetTickCount");
    CHECK(image.imports[0].ordinal == 0x1F5);
    CHECK(image.imports[0].iatRva == 0x2048u);
    CHECK(image.imports[1].dll == "WS2_32.dll");
    CHECK(image.imports[1].name.empty());
    CHECK(image.imports[1].ordinal == 23);
    CHECK(image.imports[1].iatRva == 0x2058u);
}

TEST_CASE("ParsePe rejects truncated files", "[pe]")
{
    const std::vector<uint8_t> file = TestPe();
    CHECK(ParseError({}) == "file is too short for a DOS header");
    CHECK(ParseError(std::vector<uint8_t>(file.begin(), file.begin() + 0x30)) == "file is too short for a DOS header");
    CHECK(ParseError(std::vector<uint8_t>(file.begin(), file.begin() + 0x50)) == "PE header at 0x40 is past the end of the file");
    CHECK(ParseError(std::vector<uint8_t>(file.begin(), file.begin() + 0x100)) == "optional header is truncated");
    CHECK(ParseError(std::vector<uint8_t>(file.begin(), file.begin() + 0x180)) == "section table is past the end of the file");
    CHECK(ParseError(std::vector<uint8_t>(file.begin(), file.begin() + 0x500)) == "section .idata points past the end of the file");
}

TEST_CASE("ParsePe rejects a file without MZ", "[pe]")
{
    std::vector<uint8_t> file = TestPe();
    file[0] = 'Z';
    CHECK(ParseError(file) == "no MZ signature");
}

TEST_CASE("ParsePe rejects a PE32+ file", "[pe]")
{
    std::vector<uint8_t> file = TestPe();
    file[kOptionalHeader] = 0x0B;
    file[kOptionalHeader + 1] = 0x02;
    CHECK(ParseError(file) == "PE32+ (64-bit) image; only PE32 is supported");
}

TEST_CASE("ParsePe rejects a section pointing past the end", "[pe]")
{
    std::vector<uint8_t> file = TestPe();
    Put32(&file, kSectionTable + 20, 0x10000); // .text PointerToRawData
    CHECK(ParseError(file) == "section .text points past the end of the file");
}

TEST_CASE("ParsePe rejects an import table outside the file", "[pe]")
{
    std::vector<uint8_t> file = TestPe();
    Put32(&file, kOptionalHeader + 96 + 8, 0x2F00); // import directory RVA
    CHECK(ParseError(file) == "import descriptor at RVA 0x2F00 is outside the file");
}
