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

    std::vector<std::uint8_t> pixels;
};

RgbImage readBmp(const std::string &path);

#endif