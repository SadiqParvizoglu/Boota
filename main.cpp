#include <Boota/Window/BMPWindow.h>
#include <Boota/V4L2Camera/V4L2Camera.h>
#include <Boota/BMP/BMP.h>
#include <thread>
#include <chrono>

int winWidth=640,winHeight=480;

int main()
{
    BMPWindow window(winWidth,winHeight,"Boota Camera Window");
    V4L2Camera camera("/dev/video0",winWidth,winHeight);
    BMP bmp(10,10,false);
    while (window.isOpen())
    {
        window.pollEvents();
        window.clear();

        bmp=camera.captureBMP();
        bmp.write("image.bmp");

        bmp.toGrayscale();

        window.showBMP(bmp);

        window.display();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return 0;
}
