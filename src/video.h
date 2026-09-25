#ifndef VIDEO_H
#define VIDEO_H

#include <cstddef>
#include <cstdint>
#include <string>

std::uintmax_t processVideo(const std::string &inputPath,
                            const std::string &imagePath,
                            const std::string &outputPath,
                            std::size_t width, std::size_t height,
                            std::size_t x, std::size_t y);

#endif