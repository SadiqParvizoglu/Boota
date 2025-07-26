#ifndef BOOTA_WINDOW_BMPWINDOW_H
#define BOOTA_WINDOW_BMPWINDOW_H

#include <GL/freeglut.h>
#include <Boota/BMP/BMP.h>

class BMPWindow
{
public:
    BMPWindow(int width, int height, const char* title);
    ~BMPWindow();

    bool isOpen() const;
    void pollEvents();
    void clear();
    void display();
    void close();
    void showBMP(BMP bmp);

private:
    int m_width;
    int m_height;
    const char* m_title;
    int m_windowID;
    bool m_open;

    static BMPWindow* s_instance;

    void initGL();
    static void handleKeyboard(unsigned char key, int x, int y);
    void onKeyPress(unsigned char key, int x, int y);
};

#endif
