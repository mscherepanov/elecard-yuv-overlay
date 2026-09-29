#include "bmp.h"
#include "video.h"
#include "yuv.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require(bool condition, const char *message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void put16(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint16_t value)
    {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    }

    void put32(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint32_t value)
    {
        for (std::size_t i = 0; i < 4; ++i)
        {
            bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
        }
    }

    std::vector<std::uint8_t> makeBmp(std::size_t width, std::size_t height,
                                      bool topDown, const std::vector<std::uint8_t> &rgb)
    {
        require(rgb.size() == width * height * 3, "Некорректные тестовые пиксели BMP");
        const std::size_t stride = (width * 3 + 3) & ~std::size_t(3);
        std::vector<std::uint8_t> bytes(54 + stride * height, 0);
        bytes[0] = 'B';
        bytes[1] = 'M';
        put32(bytes, 2, static_cast<std::uint32_t>(bytes.size()));
        put32(bytes, 10, 54);
        put32(bytes, 14, 40);
        put32(bytes, 18, static_cast<std::uint32_t>(width));
        put32(bytes, 22, topDown ? 0u - static_cast<std::uint32_t>(height) : static_cast<std::uint32_t>(height));
        put16(bytes, 26, 1);
        put16(bytes, 28, 24);

        for (std::size_t row = 0; row < height; ++row)
        {
            const std::size_t sourceRow = topDown ? row : height - 1 - row;
            for (std::size_t col = 0; col < width; ++col)
            {
                const std::size_t source = (sourceRow * width + col) * 3;
                const std::size_t target = 54 + row * stride + col * 3;
                bytes[target] = rgb[source + 2];
                bytes[target + 1] = rgb[source + 1];
                bytes[target + 2] = rgb[source];
            }
        }
        return bytes;
    }

    void writeFile(const std::string &path, const std::vector<std::uint8_t> &bytes)
    {
        std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(output), "Не удалось создать тестовый файл");
        output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        output.close();
        require(static_cast<bool>(output), "Не удалось записать тестовый файл");
    }

    std::vector<std::uint8_t> readFile(const std::string &path)
    {
        std::ifstream input(path.c_str(), std::ios::binary);
        require(static_cast<bool>(input), "Не удалось открыть тестовый файл");
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),
                                         std::istreambuf_iterator<char>());
    }

    void testBmp()
    {
        const std::vector<std::uint8_t> rgb = {255, 0, 0, 0, 0, 255};
        writeFile("bottom.bmp", makeBmp(1, 2, false, rgb));
        const RgbImage bottom = readBmp("bottom.bmp");
        require(bottom.width == 1 && bottom.height == 2 && bottom.pixels == rgb,
                "BMP: нижний порядок строк, BGR или padding");

        const std::vector<std::uint8_t> topRgb = {255, 0, 0, 0, 255, 0,
                                                  0, 0, 255, 255, 255, 255};
        writeFile("top.bmp", makeBmp(2, 2, true, topRgb));
        require(readBmp("top.bmp").pixels == topRgb, "BMP: верхний порядок строк");

        std::vector<std::uint8_t> compressed = makeBmp(1, 2, false, rgb);
        put32(compressed, 30, 1);
        writeFile("compressed.bmp", compressed);
        bool rejected = false;
        try
        {
            readBmp("compressed.bmp");
        }
        catch (const std::runtime_error &)
        {
            rejected = true;
        }
        require(rejected, "BMP: сжатие должно отклоняться");

        compressed = makeBmp(1, 2, false, rgb);
        compressed.pop_back();
        writeFile("truncated.bmp", compressed);
        rejected = false;
        try
        {
            readBmp("truncated.bmp");
        }
        catch (const std::runtime_error &)
        {
            rejected = true;
        }
        require(rejected, "BMP: усечённый файл должен отклоняться");
    }

    void testConversion()
    {
        const RgbImage rgb = {2, 2, {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255}};
        const Yuv420Image yuv = rgbToYuv420(rgb);
        require(yuv.width == 2 && yuv.height == 2, "YUV: размеры");
        require(yuv.y == std::vector<std::uint8_t>({81, 145, 41, 235}), "YUV: Y-плоскость");
        require(yuv.u == std::vector<std::uint8_t>({128}) &&
                    yuv.v == std::vector<std::uint8_t>({128}),
                "YUV: усреднение 2x2");

        const Yuv420Image small = rgbToYuv420Threaded(rgb, 4);
        require(small.y == yuv.y && small.u == yuv.u && small.v == yuv.v,
                "YUV: скалярный fallback");

        RgbImage patterned = {514, 258, std::vector<std::uint8_t>(514 * 258 * 3)};
        for (std::size_t i = 0; i < patterned.pixels.size(); ++i)
        {
            patterned.pixels[i] = static_cast<std::uint8_t>((i * 37 + i / 7) % 256);
        }
        const Yuv420Image scalar = rgbToYuv420(patterned);
        const Yuv420Image threaded = rgbToYuv420Threaded(patterned, 2);
        require(threaded.y == scalar.y && threaded.u == scalar.u &&
                    threaded.v == scalar.v,
                "YUV: многопоточный результат отличается от скалярного");

        bool rejected = false;
        try
        {
            rgbToYuv420(RgbImage{1, 2, std::vector<std::uint8_t>(6)});
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        require(rejected, "YUV: нечётная ширина должна отклоняться");
    }

    void testOverlay()
    {
        Yuv420Image frame = {4, 2, std::vector<std::uint8_t>(8, 10), {20, 21}, {30, 31}};
        const Yuv420Image image = {2, 2, {1, 2, 3, 4}, {5}, {6}};
        overlayYuv420(frame, image, 2, 0);
        require(frame.y == std::vector<std::uint8_t>({10, 10, 1, 2, 10, 10, 3, 4}),
                "Overlay: Y-плоскость");
        require(frame.u == std::vector<std::uint8_t>({20, 5}) &&
                    frame.v == std::vector<std::uint8_t>({30, 6}),
                "Overlay: U/V-плоскости");

        bool rejected = false;
        try
        {
            overlayYuv420(frame, image, 1, 0);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        require(rejected, "Overlay: нечётная координата должна отклоняться");

        rejected = false;
        try
        {
            overlayYuv420(frame, image, 4, 0);
        }
        catch (const std::out_of_range &)
        {
            rejected = true;
        }
        require(rejected, "Overlay: выход за границу должен отклоняться");
    }

    void testVideo()
    {
        std::vector<std::uint8_t> pixels(12, 0);
        for (std::size_t i = 0; i < 4; ++i)
        {
            pixels[i * 3] = 255;
        }
        writeFile("video-image.bmp", makeBmp(2, 2, false, pixels));

        const std::vector<std::uint8_t> first = {10, 10, 10, 10, 10, 10, 10, 10,
                                                 20, 21, 30, 31};
        const std::vector<std::uint8_t> second = {40, 40, 40, 40, 40, 40, 40, 40,
                                                  50, 51, 60, 61};
        std::vector<std::uint8_t> input = first;
        input.insert(input.end(), second.begin(), second.end());
        writeFile("video-input.yuv", input);
        require(processVideo("video-input.yuv", "video-image.bmp", "video-output.yuv",
                             4, 2, 2, 0) == 2,
                "Video: количество кадров");
        const std::vector<std::uint8_t> expected = {
            10, 10, 81, 81, 10, 10, 81, 81, 20, 90, 30, 240,
            40, 40, 81, 81, 40, 40, 81, 81, 50, 90, 60, 240};
        require(readFile("video-output.yuv") == expected, "Video: выходной I420");

        input.push_back(0);
        writeFile("video-partial.yuv", input);
        bool rejected = false;
        try
        {
            processVideo("video-partial.yuv", "video-image.bmp", "unused.yuv",
                         4, 2, 0, 0);
        }
        catch (const std::runtime_error &)
        {
            rejected = true;
        }
        require(rejected, "Video: неполный последний кадр должен отклоняться");

        writeFile("video-empty.yuv", {});
        rejected = false;
        try
        {
            processVideo("video-empty.yuv", "video-image.bmp", "unused.yuv",
                         4, 2, 0, 0);
        }
        catch (const std::runtime_error &)
        {
            rejected = true;
        }
        require(rejected, "Video: пустой файл должен отклоняться");
    }
}

int main()
{
    try
    {
        testBmp();
        testConversion();
        testOverlay();
        testVideo();
        std::remove("bottom.bmp");
        std::remove("top.bmp");
        std::remove("compressed.bmp");
        std::remove("truncated.bmp");
        std::remove("video-image.bmp");
        std::remove("video-input.yuv");
        std::remove("video-output.yuv");
        std::remove("video-partial.yuv");
        std::remove("video-empty.yuv");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}