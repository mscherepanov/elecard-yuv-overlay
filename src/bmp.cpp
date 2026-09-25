#include "bmp.h"

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace
{

    std::uint16_t read16(const std::array<std::uint8_t, 54> &header, std::size_t offset)
    {
        return static_cast<std::uint16_t>(header[offset]) |
               static_cast<std::uint16_t>(header[offset + 1]) << 8;
    }

    std::uint32_t read32(const std::array<std::uint8_t, 54> &header, std::size_t offset)
    {
        return static_cast<std::uint32_t>(header[offset]) |
               static_cast<std::uint32_t>(header[offset + 1]) << 8 |
               static_cast<std::uint32_t>(header[offset + 2]) << 16 |
               static_cast<std::uint32_t>(header[offset + 3]) << 24;
    }

} // namespace

RgbImage readBmp(const std::string &path)
{
    std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error("Не удалось открыть BMP: " + path);
    }

    const std::streamoff fileSize = file.tellg();
    if (fileSize < 54)
    {
        throw std::runtime_error("BMP слишком короткий: " + path);
    }

    file.seekg(0);
    std::array<std::uint8_t, 54> header;
    file.read(reinterpret_cast<char *>(header.data()), header.size());
    if (!file)
    {
        throw std::runtime_error("Не удалось прочитать заголовок BMP: " + path);
    }

    const std::uint32_t dibSize = read32(header, 14);
    const std::uint32_t pixelOffset = read32(header, 10);
    const std::uint32_t width = read32(header, 18);
    const std::uint32_t rawHeight = read32(header, 22);
    const bool topDown = rawHeight > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    const std::uint32_t height = topDown ? 0u - rawHeight : rawHeight;

    if (header[0] != 'B' || header[1] != 'M' || dibSize < 40 ||
        width == 0 || width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        height == 0 || height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        read16(header, 26) != 1 || read16(header, 28) != 24 || read32(header, 30) != 0)
    {
        throw std::runtime_error("Неподдерживаемый формат BMP: " + path);
    }

    const std::uint64_t dataStart = static_cast<std::uint64_t>(14) + dibSize;
    const std::uint64_t rowBytes = static_cast<std::uint64_t>(width) * 3;
    const std::uint64_t stride = (rowBytes + 3) & ~std::uint64_t(3);
    const std::uint64_t requiredSize = static_cast<std::uint64_t>(pixelOffset) + stride * height;
    if (pixelOffset < dataStart || requiredSize > static_cast<std::uint64_t>(fileSize) ||
        read32(header, 2) < requiredSize || read32(header, 2) > static_cast<std::uint64_t>(fileSize))
    {
        throw std::runtime_error("Некорректный размер BMP: " + path);
    }

    const std::uint64_t imageBytes = rowBytes * height;
    if (imageBytes > std::vector<std::uint8_t>().max_size() ||
        stride > std::vector<std::uint8_t>().max_size() ||
        stride > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
    {
        throw std::runtime_error("Слишком большой BMP: " + path);
    }

    RgbImage image = {width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(imageBytes))};
    std::vector<std::uint8_t> row(static_cast<std::size_t>(stride));
    file.seekg(pixelOffset);

    for (std::size_t fileRow = 0; fileRow < image.height; ++fileRow)
    {
        file.read(reinterpret_cast<char *>(row.data()), static_cast<std::streamsize>(stride));
        if (!file)
        {
            throw std::runtime_error("Не удалось прочитать пиксели BMP: " + path);
        }

        // BMP хранит пиксели как BGR и обычно записывает строки снизу вверх
        const std::size_t outputRow = topDown ? fileRow : image.height - 1 - fileRow;
        for (std::size_t x = 0; x < image.width; ++x)
        {
            const std::size_t source = x * 3;
            const std::size_t target = (outputRow * image.width + x) * 3;
            image.pixels[target] = row[source + 2];
            image.pixels[target + 1] = row[source + 1];
            image.pixels[target + 2] = row[source];
        }
    }

    return image;
}