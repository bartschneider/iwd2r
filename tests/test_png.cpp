// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "platform/png.h"

#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

uint32_t U32(const std::vector<unsigned char>& d, size_t at)
{
    return (uint32_t(d[at]) << 24) | (uint32_t(d[at + 1]) << 16) | (uint32_t(d[at + 2]) << 8) | uint32_t(d[at + 3]);
}

struct Chunk {
    std::string type;
    std::vector<unsigned char> data;
};

// Splits a PNG into chunks, checking each CRC.
std::vector<Chunk> ReadChunks(const std::vector<unsigned char>& d)
{
    std::vector<Chunk> chunks;
    const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    REQUIRE(d.size() >= 8);
    REQUIRE(std::equal(signature, signature + 8, d.begin()));
    size_t p = 8;
    while (p + 12 <= d.size()) {
        const uint32_t n = U32(d, p);
        REQUIRE(p + 12 + n <= d.size());
        Chunk c;
        c.type.assign(d.begin() + static_cast<std::ptrdiff_t>(p + 4), d.begin() + static_cast<std::ptrdiff_t>(p + 8));
        c.data.assign(d.begin() + static_cast<std::ptrdiff_t>(p + 8), d.begin() + static_cast<std::ptrdiff_t>(p + 8 + n));
        const uLong crc = crc32(0L, d.data() + p + 4, static_cast<uInt>(n + 4));
        CHECK(U32(d, p + 8 + n) == static_cast<uint32_t>(crc));
        chunks.push_back(c);
        p += 12 + n;
    }
    CHECK(p == d.size());
    return chunks;
}

}

TEST_CASE("png writer writes an 8-bit RGB image", "[png]")
{
    const std::string path = (std::filesystem::temp_directory_path() / ("iwd2r_png_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".png")).string();
    // 3x2 pixels with a padded pitch (4 pixels per row); the pad must not leak.
    const uint32_t pixels[8] = {
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xDEADBEEF,
        0xFF102030, 0x00FFFFFF, 0x00000000, 0xDEADBEEF
    };
    REQUIRE(platform::WritePng(path.c_str(), pixels, 3, 2, 16));

    std::ifstream in(path, std::ios::binary);
    const std::vector<unsigned char> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::filesystem::remove(path);

    const std::vector<Chunk> chunks = ReadChunks(d);
    REQUIRE(chunks.size() == 3);
    CHECK(chunks[0].type == "IHDR");
    CHECK(chunks[1].type == "IDAT");
    CHECK(chunks[2].type == "IEND");
    CHECK(chunks[2].data.empty());

    const std::vector<unsigned char>& ihdr = chunks[0].data;
    REQUIRE(ihdr.size() == 13);
    CHECK(U32(ihdr, 0) == 3);
    CHECK(U32(ihdr, 4) == 2);
    CHECK(ihdr[8] == 8); // bit depth
    CHECK(ihdr[9] == 2); // RGB
    CHECK(ihdr[10] == 0);
    CHECK(ihdr[11] == 0);
    CHECK(ihdr[12] == 0);

    std::vector<unsigned char> raw(2 * (1 + 3 * 3) + 16);
    uLongf rawSize = static_cast<uLongf>(raw.size());
    REQUIRE(uncompress(raw.data(), &rawSize, chunks[1].data.data(), static_cast<uLong>(chunks[1].data.size())) == Z_OK);
    raw.resize(rawSize);
    const std::vector<unsigned char> expected = {
        0, 0xFF, 0, 0, 0, 0xFF, 0, 0, 0, 0xFF,
        0, 0x10, 0x20, 0x30, 0xFF, 0xFF, 0xFF, 0, 0, 0
    };
    CHECK(raw == expected);
}

TEST_CASE("png writer rejects bad arguments", "[png]")
{
    const uint32_t pixel = 0;
    CHECK_FALSE(platform::WritePng(nullptr, &pixel, 1, 1, 4));
    CHECK_FALSE(platform::WritePng("x.png", nullptr, 1, 1, 4));
    CHECK_FALSE(platform::WritePng("x.png", &pixel, 0, 1, 4));
    CHECK_FALSE(platform::WritePng("x.png", &pixel, 1, 1, 2));
    const std::string missing = (std::filesystem::temp_directory_path() / "iwd2r-no-such-dir" / "x.png").string();
    CHECK_FALSE(platform::WritePng(missing.c_str(), &pixel, 1, 1, 4));
}
