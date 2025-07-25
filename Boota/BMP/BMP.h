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
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};

    Pixel() = default;
    Pixel(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};

class BMP
{
public:
    BMPFileHeader bmp_fileheader;
    BMPInfoHeader bmp_infoheader;
    BMPColorHeader bmp_colorheader;
    std::vector<uint8_t> data;

    BMP(const char *fname)
    {
        read(fname);
    }

    BMP(int32_t width, int32_t height, bool has_alpha = true)
    {
        if (width <= 0 || height <= 0)
            throw std::runtime_error("Image width and height must be positive!");

        bmp_infoheader.width = width;
        bmp_infoheader.height = height;

        if (has_alpha)
        {
            bmp_infoheader.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
            bmp_infoheader.bitcount = 32;
            bmp_infoheader.compression = 3;
            rowstride = width * 4;
            bmp_fileheader.offsetdata = sizeof(BMPFileHeader) + bmp_infoheader.size;
        }
        else
        {
            bmp_infoheader.size = sizeof(BMPInfoHeader);
            bmp_infoheader.bitcount = 24;
            bmp_infoheader.compression = 0;
            rowstride = width * 3;
            bmp_fileheader.offsetdata = sizeof(BMPFileHeader) + bmp_infoheader.size;
        }

        data.resize(rowstride * height);
        uint32_t newstride = has_alpha ? rowstride : makestridealigned(4);
        bmp_fileheader.filesize = bmp_fileheader.offsetdata + data.size();
        if (!has_alpha)
            bmp_fileheader.filesize += bmp_infoheader.height * (newstride - rowstride);
    }

    void read(const char *fname)
    {
        std::ifstream inp{fname, std::ios_base::binary};
        if (!inp)
            throw std::runtime_error("Failed to open file.");

        inp.read((char *)&bmp_fileheader, sizeof(bmp_fileheader));
        if (bmp_fileheader.signature != 0x4D42)
            throw std::runtime_error("File is not recognized as a BMP format.");

        inp.read((char *)&bmp_infoheader, sizeof(bmp_infoheader));

        if (bmp_infoheader.bitcount == 32)
        {
            if (bmp_infoheader.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColorHeader)))
            {
                inp.read((char *)&bmp_colorheader, sizeof(bmp_colorheader));
                checkcolorheader(bmp_colorheader);
            }
            else
            {
                throw std::runtime_error("File lacks bitmask information.");
            }
        }

        inp.seekg(bmp_fileheader.offsetdata, inp.beg);

        if (bmp_infoheader.height < 0)
            throw std::runtime_error("Only bottom-up BMP images are supported.");

        rowstride = bmp_infoheader.width * bmp_infoheader.bitcount / 8;
        data.resize(rowstride * bmp_infoheader.height);

        if (bmp_infoheader.bitcount == 24 && bmp_infoheader.width % 4 != 0)
        {
            uint32_t newstride = makestridealigned(4);
            std::vector<uint8_t> padding_row(newstride - rowstride);
            for (int y = 0; y < bmp_infoheader.height; ++y)
            {
                inp.read((char *)(data.data() + rowstride * y), rowstride);
                inp.read((char *)padding_row.data(), padding_row.size());
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

        if (bmp_infoheader.bitcount == 32 || (bmp_infoheader.bitcount == 24 && bmp_infoheader.width % 4 == 0))
        {
            writeheadersanddata(of);
        }
        else if (bmp_infoheader.bitcount == 24)
        {
            uint32_t newstride = makestridealigned(4);
            std::vector<uint8_t> padding_row(newstride - rowstride);
            writeheaders(of);
            for (int y = 0; y < bmp_infoheader.height; ++y)
            {
                of.write((const char *)(data.data() + rowstride * y), rowstride);
                of.write((const char *)padding_row.data(), padding_row.size());
            }
        }
        else
        {
            throw std::runtime_error("Only 24-bit and 32-bit BMP formats are supported.");
        }
    }

    void setpixel(uint32_t x, uint32_t y, const Pixel &pixel)
    {
        if (x >= static_cast<uint32_t>(bmp_infoheader.width) || y >= static_cast<uint32_t>(bmp_infoheader.height))
            throw std::runtime_error("Pixel coordinates are out of bounds.");

        uint32_t channels = bmp_infoheader.bitcount / 8;
        size_t idx = channels * (y * bmp_infoheader.width + x);
        data[idx + 0] = pixel.b;
        data[idx + 1] = pixel.g;
        data[idx + 2] = pixel.r;
        if (channels == 4)
            data[idx + 3] = 255;
    }

    Pixel getpixel(uint32_t x, uint32_t y)
    {
        if (x >= static_cast<uint32_t>(bmp_infoheader.width) || y >= static_cast<uint32_t>(bmp_infoheader.height))
            throw std::runtime_error("Pixel coordinates are out of bounds.");

        uint32_t channels = bmp_infoheader.bitcount / 8;
        size_t idx = channels * (y * bmp_infoheader.width + x);

        return Pixel(data[idx + 2], data[idx + 1], data[idx + 0]);
    }

private:
    uint32_t rowstride{0};

    uint32_t makestridealigned(uint32_t alignstride)
    {
        uint32_t newstride = rowstride;
        while (newstride % alignstride != 0)
            newstride++;
        return newstride;
    }

    void writeheaders(std::ofstream &of)
    {
        of.write((const char *)&bmp_fileheader, sizeof(bmp_fileheader));
        of.write((const char *)&bmp_infoheader, sizeof(bmp_infoheader));
        if (bmp_infoheader.bitcount == 32)
            of.write((const char *)&bmp_colorheader, sizeof(bmp_colorheader));
    }

    void writeheadersanddata(std::ofstream &of)
    {
        writeheaders(of);
        of.write((const char *)data.data(), data.size());
    }

    void checkcolorheader(BMPColorHeader &bmp_colorheader)
    {
        BMPColorHeader expectedcolorheader;
        if (expectedcolorheader.redmask != bmp_colorheader.redmask ||
            expectedcolorheader.greenmask != bmp_colorheader.greenmask ||
            expectedcolorheader.bluemask != bmp_colorheader.bluemask ||
            expectedcolorheader.alphamask != bmp_colorheader.alphamask)
        {
            throw std::runtime_error("Pixel data must be in BGRA format.");
        }

        if (expectedcolorheader.colorspacetype != bmp_colorheader.colorspacetype)
        {
            throw std::runtime_error("Color space must be sRGB.");
        }
    }
};
