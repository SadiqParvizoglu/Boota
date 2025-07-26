#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#pragma pack(push, 1)
struct BMPFileHeader
{
    uint16_t signature{0x4D42};
    uint32_t filesize{0};
    uint16_t reserved1{0};
    uint16_t reserved2{0};
    uint32_t offsetdata{0};
};

struct BMPInfoHeader
{
    uint32_t size{0};
    int32_t width{0};
    int32_t height{0};
    uint16_t planes{1};
    uint16_t bitcount{0};
    uint32_t compression{0};
    uint32_t imagesize{0};
    int32_t xpixels_perM{0};
    int32_t ypixels_perM{0};
    uint32_t colorsused{0};
    uint32_t colorsimportant{0};
};

struct BMPColorHeader
{
    uint32_t redmask{0x00ff0000};
    uint32_t greenmask{0x0000ff00};
    uint32_t bluemask{0x000000ff};
    uint32_t alphamask{0xff000000};
    uint32_t colorspacetype{0x73524742};
    uint32_t unused[16]{0};
};
#pragma pack(pop)

struct Pixel
{
    uint8_t red{0};
    uint8_t green{0};
    uint8_t blue{0};

    Pixel() = default;
    Pixel(uint8_t red, uint8_t green, uint8_t blue) : red(red), green(green), blue(blue) {}
};

class BMP
{
public:
    BMPFileHeader bmpFileHeader;
    BMPInfoHeader bmpInfoHeader;
    BMPColorHeader bmpColorHeader;
    std::vector<uint8_t> data;

    BMP(const char *fname)
    {
        read(fname);
    }

    BMP(int32_t width, int32_t height, bool hasAlpha = true)
    {
        if (width <= 0 || height <= 0)
            throw std::runtime_error("Image width and height must be positive!");

        bmpInfoHeader.width = width;
        bmpInfoHeader.height = height;

        if (hasAlpha)
        {
            bmpInfoHeader.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
            bmpInfoHeader.bitcount = 32;
            bmpInfoHeader.compression = 3;
            rowStride = width * 4;
            bmpFileHeader.offsetdata = sizeof(BMPFileHeader) + bmpInfoHeader.size;
        }
        else
        {
            bmpInfoHeader.size = sizeof(BMPInfoHeader);
            bmpInfoHeader.bitcount = 24;
            bmpInfoHeader.compression = 0;
            rowStride = width * 3;
            bmpFileHeader.offsetdata = sizeof(BMPFileHeader) + bmpInfoHeader.size;
        }

        data.resize(rowStride * height);
        uint32_t newStride = hasAlpha ? rowStride : makeStrideAligned(4);
        bmpFileHeader.filesize = bmpFileHeader.offsetdata + data.size();
        if (!hasAlpha)
            bmpFileHeader.filesize += bmpInfoHeader.height * (newStride - rowStride);
    }

    void read(const char *fname)
    {
        std::ifstream inp{fname, std::ios_base::binary};
        if (!inp)
            throw std::runtime_error("Failed to open file.");

        inp.read((char *)&bmpFileHeader, sizeof(bmpFileHeader));
        if (bmpFileHeader.signature != 0x4D42)
            throw std::runtime_error("File is not recognized as a BMP format.");

        inp.read((char *)&bmpInfoHeader, sizeof(bmpInfoHeader));

        if (bmpInfoHeader.bitcount == 32)
        {
            if (bmpInfoHeader.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColorHeader)))
            {
                inp.read((char *)&bmpColorHeader, sizeof(bmpColorHeader));
                checkColorHeader(bmpColorHeader);
            }
            else
            {
                throw std::runtime_error("File lacks bitmask information.");
            }
        }

        inp.seekg(bmpFileHeader.offsetdata, inp.beg);

        if (bmpInfoHeader.height < 0)
            throw std::runtime_error("Only bottom-up BMP images are supported.");

        rowStride = bmpInfoHeader.width * bmpInfoHeader.bitcount / 8;
        data.resize(rowStride * bmpInfoHeader.height);

        if (bmpInfoHeader.bitcount == 24 && bmpInfoHeader.width % 4 != 0)
        {
            uint32_t newStride = makeStrideAligned(4);
            std::vector<uint8_t> paddingRow(newStride - rowStride);
            for (int y = 0; y < bmpInfoHeader.height; ++y)
            {
                inp.read((char *)(data.data() + rowStride * y), rowStride);
                inp.read((char *)paddingRow.data(), paddingRow.size());
            }
        }
        else
        {
            inp.read((char *)data.data(), data.size());
        }
    }

    void write(const char *fname)
    {
        std::ofstream of{fname, std::ios_base::binary};
        if (!of)
            throw std::runtime_error("Failed to create file.");

        if (bmpInfoHeader.bitcount == 32 || (bmpInfoHeader.bitcount == 24 && bmpInfoHeader.width % 4 == 0))
        {
            writeHeadersAndData(of);
        }
        else if (bmpInfoHeader.bitcount == 24)
        {
            uint32_t newStride = makeStrideAligned(4);
            std::vector<uint8_t> paddingRow(newStride - rowStride);
            writeHeaders(of);
            for (int y = 0; y < bmpInfoHeader.height; ++y)
            {
                of.write((const char *)(data.data() + rowStride * y), rowStride);
                of.write((const char *)paddingRow.data(), paddingRow.size());
            }
        }
        else
        {
            throw std::runtime_error("Only 24-bit and 32-bit BMP formats are supported.");
        }
    }

    void setPixel(uint32_t x, uint32_t y, const Pixel &pixel)
    {
        if (x >= static_cast<uint32_t>(bmpInfoHeader.width) || y >= static_cast<uint32_t>(bmpInfoHeader.height))
            throw std::runtime_error("Pixel coordinates are out of bounds.");

        uint32_t channels = bmpInfoHeader.bitcount / 8;
        size_t idx = channels * (y * bmpInfoHeader.width + x);
        data[idx + 0] = pixel.blue;
        data[idx + 1] = pixel.green;
        data[idx + 2] = pixel.red;
        if (channels == 4)
            data[idx + 3] = 255;
    }

    Pixel getPixel(uint32_t x, uint32_t y)
    {
        if (x >= static_cast<uint32_t>(bmpInfoHeader.width) || y >= static_cast<uint32_t>(bmpInfoHeader.height))
            throw std::runtime_error("Pixel coordinates are out of bounds.");

        uint32_t channels = bmpInfoHeader.bitcount / 8;
        size_t idx = channels * (y * bmpInfoHeader.width + x);

        return Pixel(data[idx + 2], data[idx + 1], data[idx + 0]);
    }

    void toGrayscale()
    {
        uint32_t channels = bmpInfoHeader.bitcount / 8;
        if (channels != 3 && channels != 4)
            throw std::runtime_error("Grayscale conversion only supports 24-bit or 32-bit BMP images.");

        for (int y = 0; y < bmpInfoHeader.height; ++y)
        {
            for (int x = 0; x < bmpInfoHeader.width; ++x)
            {
                size_t idx = channels * (y * bmpInfoHeader.width + x);
                uint8_t blue = data[idx + 0];
                uint8_t green = data[idx + 1];
                uint8_t red = data[idx + 2];

                uint8_t gray = static_cast<uint8_t>(0.299 * red + 0.587 * green + 0.114 * blue);
                data[idx + 0] = gray;
                data[idx + 1] = gray;
                data[idx + 2] = gray;
            }
        }
    }

private:
    uint32_t rowStride{0};

    uint32_t makeStrideAligned(uint32_t alignStride)
    {
        uint32_t newStride = rowStride;
        while (newStride % alignStride != 0)
            newStride++;
        return newStride;
    }

    void writeHeaders(std::ofstream &of)
    {
        of.write((const char *)&bmpFileHeader, sizeof(bmpFileHeader));
        of.write((const char *)&bmpInfoHeader, sizeof(bmpInfoHeader));
        if (bmpInfoHeader.bitcount == 32)
            of.write((const char *)&bmpColorHeader, sizeof(bmpColorHeader));
    }

    void writeHeadersAndData(std::ofstream &of)
    {
        writeHeaders(of);
        of.write((const char *)data.data(), data.size());
    }

    void checkColorHeader(BMPColorHeader &colorHeader)
    {
        BMPColorHeader expectedColorHeader;
        if (expectedColorHeader.redmask != colorHeader.redmask ||
            expectedColorHeader.greenmask != colorHeader.greenmask ||
            expectedColorHeader.bluemask != colorHeader.bluemask ||
            expectedColorHeader.alphamask != colorHeader.alphamask)
        {
            throw std::runtime_error("Pixel data must be in BGRA format.");
        }

        if (expectedColorHeader.colorspacetype != colorHeader.colorspacetype)
        {
            throw std::runtime_error("Color space must be sRGB.");
        }
    }
};
