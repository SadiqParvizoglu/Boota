#include <bits/stdc++.h>
#include <Boota/BMP/BMP.h>
#include <Boota/V4L2Camera/V4L2Camera.h>
using namespace std;

int main()
{
    V4L2Camera camera("/dev/video0", 640, 480);
    BMP bmp = camera.captureBMP();
    bmp.write("capture.bmp");
    return 0;
}
