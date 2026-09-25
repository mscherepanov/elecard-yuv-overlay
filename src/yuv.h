#ifndef YUV_H
#define YUV_H

#include "bmp.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct Yuv420Image
{
    std::size_t width;
    std::size_t height;

    std::vector<std::uint8_t> y;
    std::vector<std::uint8_t> u;
    std::vector<std::uint8_t> v;
};

Yuv420Image rgbToYuv420(const RgbImage &image);
void overlayYuv420(Yuv420Image &frame, const Yuv420Image &image,
                   std::size_t x, std::size_t y);

#endif