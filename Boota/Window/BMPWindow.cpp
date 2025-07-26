#include <Boota/Window/BMPWindow.h>
#include <Boota/BMP/BMP.h>

BMPWindow* BMPWindow::s_instance = nullptr;

BMPWindow::BMPWindow(int width, int height, const char* title)
    : m_width(width), m_height(height), m_title(title), m_open(true)
{
    int argc = 1;
    char* argv[1] = { const_cast<char*>("BMPWindow") };
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(m_width, m_height);
    m_windowID = glutCreateWindow(m_title);

    glutDisplayFunc([](){});

    s_instance = this;
    glutKeyboardFunc(handleKeyboard);

    initGL();
}


BMPWindow::~BMPWindow()
{
    close();
}

void BMPWindow::initGL()
{
    glClearColor(0, 0, 0, 0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, m_width, 0, m_height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

bool BMPWindow::isOpen() const
{
    return m_open;
}

void BMPWindow::pollEvents()
{
    glutMainLoopEvent();
}

void BMPWindow::clear()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

void BMPWindow::showBMP(BMP bmp)
{
    glBegin(GL_POINTS);
    for (int y=0;y<bmp.bmp_infoheader.height;y++)
    {
        for (int x=0;x<bmp.bmp_infoheader.width;x++)
        {
            Pixel pixel=bmp.getpixel(x,y);
            glColor3d(pixel.red/255.0,pixel.green/255.0,pixel.blue/255.0);
            glVertex2i(x,y);
        }
    }
    glEnd();
}

void BMPWindow::display()
{
    glutSwapBuffers();
}

void BMPWindow::close()
{
    m_open = false;
}

void BMPWindow::handleKeyboard(unsigned char key, int x, int y)
{
    if (s_instance)
        s_instance->onKeyPress(key, x, y);
}

void BMPWindow::onKeyPress(unsigned char key, int, int)
{
    if (key == 27)
        close();
}
