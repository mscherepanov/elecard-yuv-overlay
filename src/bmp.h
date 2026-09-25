#ifndef BMP_H
#define BMP_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct RgbImage
{
    std::size_t width;
    std::size_t height;
    // Строки идут сверху вниз; каждый пиксель хранится как R, G, B
    std::vector<std::uint8_t> pixels;
};

RgbImage readBmp(const std::string &path);

#endif