#include "video.h"

#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    std::size_t parseSize(const char *text, const char *name)
    {
        if (*text == '\0')
        {
            throw std::invalid_argument(std::string("Пустое значение: ") + name);
        }

        std::size_t value = 0;
        const std::size_t maxSize = std::numeric_limits<std::size_t>::max();
        for (const char *current = text; *current != '\0'; ++current)
        {
            if (*current < '0' || *current > '9')
            {
                throw std::invalid_argument(std::string("Некорректное значение: ") + name);
            }
            const std::size_t digit = static_cast<std::size_t>(*current - '0');
            if (value > (maxSize - digit) / 10)
            {
                throw std::invalid_argument(std::string("Слишком большое значение: ") + name);
            }
            value = value * 10 + digit;
        }
        return value;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 6 && argc != 8)
    {
        std::cerr << "Usage: " << argv[0]
                  << " input.yuv image.bmp output.yuv width height [x y]\n";
        return 1;
    }

    try
    {
        const std::size_t width = parseSize(argv[4], "width");
        const std::size_t height = parseSize(argv[5], "height");
        const std::size_t x = argc == 8 ? parseSize(argv[6], "x") : 0;
        const std::size_t y = argc == 8 ? parseSize(argv[7], "y") : 0;
        const std::uintmax_t count = processVideo(argv[1], argv[2], argv[3],
                                                  width, height, x, y);
        std::cout << "Обработано кадров: " << count << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}