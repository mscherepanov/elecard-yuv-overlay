#include "video.h"

#include "bmp.h"
#include "yuv.h"

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
    void readPlane(std::ifstream &input, std::vector<std::uint8_t> &plane)
    {
        const std::streamsize count = static_cast<std::streamsize>(plane.size());
        input.read(reinterpret_cast<char *>(plane.data()), count);
        if (input.gcount() != count || input.bad())
        {
            throw std::runtime_error(input.eof() ? "Неполный кадр во входном YUV-файле"
                                                 : "Ошибка чтения входного YUV-файла");
        }
    }

    void writePlane(std::ofstream &output, const std::vector<std::uint8_t> &plane)
    {
        output.write(reinterpret_cast<const char *>(plane.data()),
                     static_cast<std::streamsize>(plane.size()));
        if (!output)
        {
            throw std::runtime_error("Ошибка записи выходного YUV-файла");
        }
    }
}

std::uintmax_t processVideo(const std::string &inputPath,
                            const std::string &imagePath,
                            const std::string &outputPath,
                            std::size_t width, std::size_t height,
                            std::size_t x, std::size_t y)
{
    if (outputPath == inputPath || outputPath == imagePath)
    {
        throw std::invalid_argument("Выходной файл должен отличаться от входных файлов");
    }
    if (width == 0 || height == 0 || width % 2 != 0 || height % 2 != 0)
    {
        throw std::invalid_argument("Размеры кадра должны быть положительными и чётными");
    }

    const std::size_t maxSize = std::numeric_limits<std::size_t>::max();
    if (width > maxSize / height)
    {
        throw std::invalid_argument("Слишком большие размеры кадра");
    }
    const std::size_t ySize = width * height;
    const std::size_t chromaSize = ySize / 4;
    if (ySize > maxSize - 2 * chromaSize ||
        static_cast<std::uintmax_t>(ySize) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
    {
        throw std::invalid_argument("Слишком большой размер кадра");
    }
    const std::size_t frameSize = ySize + 2 * chromaSize;

    std::ifstream input(inputPath.c_str(), std::ios::binary | std::ios::ate);
    if (!input)
    {
        throw std::runtime_error("Не удалось открыть входной YUV-файл: " + inputPath);
    }
    const std::streamoff inputSize = input.tellg();
    if (inputSize <= 0)
    {
        throw std::runtime_error("Входной YUV-файл пуст или его размер недоступен");
    }
    if (static_cast<std::uintmax_t>(frameSize) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::streamoff>::max()) ||
        inputSize % static_cast<std::streamoff>(frameSize) != 0)
    {
        throw std::runtime_error("Входной YUV-файл содержит неполный кадр");
    }
    const std::streamoff frameCount = inputSize / static_cast<std::streamoff>(frameSize);
    input.seekg(0);
    if (!input)
    {
        throw std::runtime_error("Не удалось перейти к началу входного YUV-файла");
    }

    const Yuv420Image image = rgbToYuv420(readBmp(imagePath));
    Yuv420Image frame = {width, height,
                         std::vector<std::uint8_t>(ySize),
                         std::vector<std::uint8_t>(chromaSize),
                         std::vector<std::uint8_t>(chromaSize)};
    overlayYuv420(frame, image, x, y);

    std::ofstream output(outputPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("Не удалось открыть выходной YUV-файл: " + outputPath);
    }

    for (std::streamoff index = 0; index < frameCount; ++index)
    {
        readPlane(input, frame.y);
        readPlane(input, frame.u);
        readPlane(input, frame.v);
        overlayYuv420(frame, image, x, y);
        writePlane(output, frame.y);
        writePlane(output, frame.u);
        writePlane(output, frame.v);
    }
    output.close();
    if (!output)
    {
        throw std::runtime_error("Не удалось завершить запись выходного YUV-файла");
    }

    return static_cast<std::uintmax_t>(frameCount);
}