#include "yuv.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <thread>

namespace
{
    std::int64_t roundDiv(std::int64_t numerator, std::int64_t denominator)
    {
        if (numerator < 0)
        {
            return -((-numerator + denominator / 2) / denominator);
        }
        return (numerator + denominator / 2) / denominator;
    }

    std::uint8_t clampByte(std::int64_t value, std::int64_t lower, std::int64_t upper)
    {
        if (value < lower)
        {
            value = lower;
        }
        if (value > upper)
        {
            value = upper;
        }
        return static_cast<std::uint8_t>(value);
    }

    std::uint8_t luma(std::uint8_t r, std::uint8_t g, std::uint8_t b)
    {
        const std::int64_t weighted = 299LL * r + 587LL * g + 114LL * b;
        return clampByte(16 + roundDiv(219 * weighted, 255000), 16, 235);
    }

    std::uint8_t chromaU(std::int64_t r, std::int64_t g, std::int64_t b)
    {
        const std::int64_t weighted = -168736 * r - 331264 * g + 500000 * b;
        return clampByte(128 + roundDiv(224 * weighted, 4 * 255000000LL), 16, 240);
    }

    std::uint8_t chromaV(std::int64_t r, std::int64_t g, std::int64_t b)
    {
        const std::int64_t weighted = 500000 * r - 418688 * g - 81312 * b;
        return clampByte(128 + roundDiv(224 * weighted, 4 * 255000000LL), 16, 240);
    }

    void validateRgb(const RgbImage &image)
    {
        if (image.width == 0 || image.height == 0 ||
            image.width % 2 != 0 || image.height % 2 != 0)
        {
            throw std::invalid_argument("Размеры RGB-изображения должны быть положительными и чётными");
        }

        const std::size_t maxSize = std::numeric_limits<std::size_t>::max();
        if (image.width > maxSize / image.height ||
            image.width * image.height > maxSize / 3 ||
            image.pixels.size() != image.width * image.height * 3)
        {
            throw std::invalid_argument("Некорректный размер данных RGB-изображения");
        }
    }

    Yuv420Image allocateYuv(const RgbImage &image)
    {
        const std::size_t ySize = image.width * image.height;
        const std::size_t chromaSize = ySize / 4;
        return {image.width, image.height,
                std::vector<std::uint8_t>(ySize),
                std::vector<std::uint8_t>(chromaSize),
                std::vector<std::uint8_t>(chromaSize)};
    }

    void convertBlockRows(const RgbImage &image, Yuv420Image &result,
                          std::size_t first, std::size_t last)
    {
        // Каждый диапазон содержит целые строки блоков 2x2 и независимые элементы U/V.
        for (std::size_t blockRow = first; blockRow < last; ++blockRow)
        {
            const std::size_t row = blockRow * 2;
            for (std::size_t col = 0; col < image.width; col += 2)
            {
                const std::size_t firstPixel = (row * image.width + col) * 3;
                const std::size_t indices[4] = {firstPixel, firstPixel + 3,
                                                firstPixel + image.width * 3,
                                                firstPixel + image.width * 3 + 3};
                std::int64_t sumR = 0;
                std::int64_t sumG = 0;
                std::int64_t sumB = 0;

                for (std::size_t i = 0; i < 4; ++i)
                {
                    const std::size_t source = indices[i];
                    const std::size_t target = (row + i / 2) * image.width + col + i % 2;
                    const std::uint8_t r = image.pixels[source];
                    const std::uint8_t g = image.pixels[source + 1];
                    const std::uint8_t b = image.pixels[source + 2];
                    result.y[target] = luma(r, g, b);
                    sumR += r;
                    sumG += g;
                    sumB += b;
                }

                const std::size_t chromaIndex = blockRow * (image.width / 2) + col / 2;
                result.u[chromaIndex] = chromaU(sumR, sumG, sumB);
                result.v[chromaIndex] = chromaV(sumR, sumG, sumB);
            }
        }
    }

    void joinThreads(std::vector<std::thread> &threads)
    {
        for (std::size_t i = 0; i < threads.size(); ++i)
        {
            if (threads[i].joinable())
            {
                threads[i].join();
            }
        }
    }

    bool validYuv420(const Yuv420Image &image)
    {
        if (image.width == 0 || image.height == 0 ||
            image.width % 2 != 0 || image.height % 2 != 0 ||
            image.width > std::numeric_limits<std::size_t>::max() / image.height)
        {
            return false;
        }

        const std::size_t ySize = image.width * image.height;
        const std::size_t chromaSize = ySize / 4;
        return image.y.size() == ySize &&
               image.u.size() == chromaSize && image.v.size() == chromaSize;
    }

    void copyPlane(std::vector<std::uint8_t> &destination, std::size_t destinationWidth,
                   const std::vector<std::uint8_t> &source, std::size_t sourceWidth,
                   std::size_t sourceHeight, std::size_t x, std::size_t y)
    {
        for (std::size_t row = 0; row < sourceHeight; ++row)
        {
            const std::size_t sourceOffset = row * sourceWidth;
            const std::size_t destinationOffset = (y + row) * destinationWidth + x;
            std::copy(source.begin() + sourceOffset,
                      source.begin() + sourceOffset + sourceWidth,
                      destination.begin() + destinationOffset);
        }
    }
}

Yuv420Image rgbToYuv420(const RgbImage &image)
{
    validateRgb(image);
    Yuv420Image result = allocateYuv(image);
    convertBlockRows(image, result, 0, image.height / 2);
    return result;
}

Yuv420Image rgbToYuv420Threaded(const RgbImage &image, unsigned workerCount)
{
    validateRgb(image);

    const std::size_t requested = workerCount == 0
                                      ? std::thread::hardware_concurrency()
                                      : workerCount;
    const std::size_t blockRows = image.height / 2;
    const std::size_t useful = image.width * image.height / 65536;
    const std::size_t workers = std::min(requested, std::min(blockRows, useful));
    if (workers < 2)
    {
        return rgbToYuv420(image);
    }

    Yuv420Image result = allocateYuv(image);
    std::vector<std::thread> threads;
    threads.reserve(workers - 1);

    const std::size_t rowsPerWorker = blockRows / workers;
    const std::size_t extraRows = blockRows % workers;
    std::size_t begin = 0;
    try
    {
        for (std::size_t i = 0; i + 1 < workers; ++i)
        {
            const std::size_t end = begin + rowsPerWorker + (i < extraRows ? 1 : 0);
            threads.emplace_back([&image, &result, begin, end]()
                                 { convertBlockRows(image, result, begin, end); });
            begin = end;
        }
        convertBlockRows(image, result, begin, blockRows);
    }
    catch (...)
    {
        joinThreads(threads);
        throw;
    }
    joinThreads(threads);
    return result;
}

void overlayYuv420(Yuv420Image &frame, const Yuv420Image &image,
                   std::size_t x, std::size_t y)
{
    if (!validYuv420(frame) || !validYuv420(image))
    {
        throw std::invalid_argument("Некорректные размеры или плоскости YUV420");
    }
    if (x % 2 != 0 || y % 2 != 0)
    {
        throw std::invalid_argument("Координаты наложения должны быть чётными");
    }
    if (image.width > frame.width || image.height > frame.height ||
        x > frame.width - image.width || y > frame.height - image.height)
    {
        throw std::out_of_range("Картинка выходит за границы кадра");
    }
    if (&frame == &image)
    {
        return;
    }

    copyPlane(frame.y, frame.width, image.y, image.width, image.height, x, y);
    copyPlane(frame.u, frame.width / 2, image.u, image.width / 2,
              image.height / 2, x / 2, y / 2);
    copyPlane(frame.v, frame.width / 2, image.v, image.width / 2,
              image.height / 2, x / 2, y / 2);
}