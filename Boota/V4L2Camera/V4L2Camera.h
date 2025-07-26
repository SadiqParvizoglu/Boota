#pragma once

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#include <stdexcept>
#include <cstring>
#include <vector>
#include <iostream>

#include <Boota/BMP/BMP.h>

class V4L2Camera
{
public:
    V4L2Camera(const char* device = "/dev/video0", uint32_t width = 640, uint32_t height = 480)
        : fd(-1), width(width), height(height)
    {
        openDevice(device);
        initDevice();
        startCapturing();
    }

    ~V4L2Camera()
    {
        stopCapturing();
        closeDevice();
    }

    BMP captureBMP()
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeval tv = {0};
        tv.tv_sec = 2;

        int r = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (r <= 0)
            throw std::runtime_error("Timeout or error waiting for frame");

        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1)
            throw std::runtime_error("Failed to dequeue buffer");

        std::vector<uint8_t> rgb24(width * height * 3);
        yuyvToRgb24((uint8_t*)buffers[buf.index].start, rgb24.data(), width, height);

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
            throw std::runtime_error("Failed to requeue buffer");

        BMP bmp(width, height, false);
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                size_t i = (y * width + x) * 3;
                bmp.setPixel(width - x - 1, height - y - 1, Pixel(rgb24[i], rgb24[i+1], rgb24[i+2]));
            }
        }
        return bmp;
    }

private:
    struct Buffer {
        void* start;
        size_t length;
    };

    int fd;
    uint32_t width;
    uint32_t height;
    std::vector<Buffer> buffers;

    void openDevice(const char* device)
    {
        fd = open(device, O_RDWR);
        if (fd == -1)
            throw std::runtime_error("Cannot open device");
    }

    void closeDevice()
    {
        if (fd != -1)
            close(fd);
        fd = -1;
    }

    void initDevice()
    {
        v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
            throw std::runtime_error("Failed to query device capabilities");

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
            throw std::runtime_error("Device does not support capture");

        if (!(cap.capabilities & V4L2_CAP_STREAMING))
            throw std::runtime_error("Device does not support streaming");

        v4l2_format fmt = {};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;

        if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
            throw std::runtime_error("Failed to set format");

        if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
            throw std::runtime_error("Device did not accept YUYV format");

        width = fmt.fmt.pix.width;
        height = fmt.fmt.pix.height;

        v4l2_requestbuffers req = {};
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
            throw std::runtime_error("Failed to request buffers");

        buffers.resize(req.count);

        for (size_t i = 0; i < buffers.size(); ++i)
        {
            v4l2_buffer buf = {};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
                throw std::runtime_error("Failed to query buffer");

            buffers[i].length = buf.length;
            buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
            if (buffers[i].start == MAP_FAILED)
                throw std::runtime_error("Failed to mmap buffer");
        }

        for (size_t i = 0; i < buffers.size(); ++i)
        {
            v4l2_buffer buf = {};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
                throw std::runtime_error("Failed to queue buffer");
        }
    }

    void startCapturing()
    {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_STREAMON, &type) == -1)
            throw std::runtime_error("Failed to start capture");
    }

    void stopCapturing()
    {
        if (fd == -1)
            return;

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        for (auto &buf : buffers)
            munmap(buf.start, buf.length);
    }

    static void yuyvToRgb24(uint8_t* yuyv, uint8_t* rgb, int width, int height)
    {
        for (int i = 0, j = 0; i < width * height * 2; i += 4, j += 6)
        {
            uint8_t y0 = yuyv[i + 0];
            uint8_t u = yuyv[i + 1];
            uint8_t y1 = yuyv[i + 2];
            uint8_t v = yuyv[i + 3];

            yuvToRgbPixel(y0, u, v, &rgb[j], &rgb[j + 1], &rgb[j + 2]);
            yuvToRgbPixel(y1, u, v, &rgb[j + 3], &rgb[j + 4], &rgb[j + 5]);
        }
    }

    static void yuvToRgbPixel(uint8_t y, uint8_t u, uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b)
    {
        int c = y - 16;
        int d = u - 128;
        int e = v - 128;

        int rt = (298 * c + 409 * e + 128) >> 8;
        int gt = (298 * c - 100 * d - 208 * e + 128) >> 8;
        int bt = (298 * c + 516 * d + 128) >> 8;

        *r = std::clamp(rt, 0, 255);
        *g = std::clamp(gt, 0, 255);
        *b = std::clamp(bt, 0, 255);
    }
};

