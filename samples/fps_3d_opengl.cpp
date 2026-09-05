#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

namespace {
constexpr int width = 1280;
constexpr int height = 720;
constexpr double pi = 3.14159265358979323846;
const std::array<const char*, 24> map = {
    "################################",
    "#..............##..............#",
    "#..######......##......######..#",
    "#..#....#......##......#....#..#",
    "#..#....#..............#....#..#",
    "#..######..####..####..######..#",
    "#..........#..........#........#",
    "#..........#..######..#........#",
    "#..####....#..#....#..#....####",
    "#..#.......#..#....#..#.......#",
    "#..#.......#..######..#.......#",
    "#..####....#..........#....####",
    "#..............##..............#",
    "#..####..####..##..####..####..#",
    "#..#......#....##....#......#..#",
    "#..#......#..........#......#..#",
    "#..####..######..######..####..#",
    "#........#............#........#",
    "#........#..########..#........#",
    "#........#..#......#..#........#",
    "#........#..#......#..#........#",
    "#........#..########..#........#",
    "#..............................#",
    "################################"};
struct Cube { double x, z; bool alive; };
struct Medkit { double x, z; bool available; };
HWND windowHandle = nullptr;
HDC deviceContext = nullptr;
HGLRC renderContext = nullptr;
std::array<bool, 256> keys = {};
std::array<Cube, 12> enemies = {{{8.5, 2.5, true}, {11.5, 4.5, true}, {14.5, 7.5, true},
                                  {18.5, 2.5, true}, {23.5, 4.5, true}, {28.5, 2.5, true},
                                  {5.5, 9.5, true}, {13.5, 10.5, true}, {20.5, 9.5, true},
                                  {26.5, 10.5, true}, {10.5, 17.5, true}, {23.5, 18.5, true}}};
std::array<Medkit, 6> medkits = {{{6.5, 1.5, true}, {25.5, 1.5, true}, {6.5, 6.5, true},
                                   {25.5, 6.5, true}, {8.5, 22.5, true}, {23.5, 22.5, true}}};
double playerX = 2.5, playerZ = 2.5, playerAngle = 0.0, jump = 0.0, jumpSpeed = 0.0, fireTimer = 0.0, fireFlash = 0.0;
int frameCount = 0, displayedFps = 0;
std::chrono::steady_clock::time_point fpsClock;
int previousMouseX = -1;
int playerHealth = 100;
GLuint brickTexture = 0;
GLuint groundTexture = 0;
GLuint skyTexture = 0;
GLuint fontBase = 0;

bool IsWall(double x, double z) {
    const int mapX = static_cast<int>(x), mapZ = static_cast<int>(z);
    return mapZ < 0 || mapZ >= static_cast<int>(map.size()) || mapX < 0 || mapX >= 32 || map[mapZ][mapX] == '#';
}

bool PlayerPositionBlocked(double x, double z)
{
    constexpr double playerRadius = 0.28;
    return IsWall(x - playerRadius, z - playerRadius) ||
           IsWall(x + playerRadius, z - playerRadius) ||
           IsWall(x - playerRadius, z + playerRadius) ||
           IsWall(x + playerRadius, z + playerRadius);
}

double WallDistanceAlongAim()
{
    for (double distance = 0.2; distance < 20.0; distance += 0.05)
        if (IsWall(playerX + cos(playerAngle) * distance, playerZ + sin(playerAngle) * distance))
            return distance;
    return 20.0;
}

void CreateBrickTexture()
{
    std::array<std::uint32_t, 64 * 64> pixels = {};
    for (int y = 0; y < 64; ++y)
    {
        const int brickRow = y / 16;
        const int offset = (brickRow % 2) * 16;
        for (int x = 0; x < 64; ++x)
        {
            const bool mortar = (y % 16 < 2) || ((x + offset) % 32 < 2);
            pixels[y * 64 + x] = mortar ? 0xff30251f : 0xff9b4b32;
        }
    }
    glGenTextures(1, &brickTexture);
    glBindTexture(GL_TEXTURE_2D, brickTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}

void CreateHudFont()
{
    fontBase = glGenLists(96);
    HFONT font = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, "Consolas");
    HFONT oldFont = static_cast<HFONT>(SelectObject(deviceContext, font));
    wglUseFontBitmapsA(deviceContext, 32, 96, fontBase);
    SelectObject(deviceContext, oldFont);
    DeleteObject(font);
}

GLuint CreateSurfaceTexture(const std::array<std::uint32_t, 64 * 64>& pixels)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

void CreateEnvironmentTextures()
{
    std::array<std::uint32_t, 64 * 64> groundPixels = {};
    std::array<std::uint32_t, 64 * 64> skyPixels = {};
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            const int concreteNoise = ((x * 17 + y * 31 + x * y) % 17) - 8;
            const bool seam = (x % 32 == 0) || (y % 32 == 0);
            const int concrete = seam ? 155 : std::clamp(218 + concreteNoise, 190, 235);
            groundPixels[y * 64 + x] = 0xff000000 |
                                        (static_cast<std::uint32_t>(concrete) << 16) |
                                        (static_cast<std::uint32_t>(concrete) << 8) |
                                        static_cast<std::uint32_t>(concrete);
            const int sky = 90 + (63 - y) * 80 / 63;
            skyPixels[y * 64 + x] = 0xff000000 |
                                     (static_cast<std::uint32_t>(sky / 2) << 16) |
                                     (static_cast<std::uint32_t>(sky) << 8) |
                                     static_cast<std::uint32_t>(sky + 25);
        }
    }
    groundTexture = CreateSurfaceTexture(groundPixels);
    skyTexture = CreateSurfaceTexture(skyPixels);
}

void CubeMesh(double x, double y, double z, double size, float r, float g, float b) {
    const double h = size / 2.0;
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex3d(x-h,y-h,z+h); glVertex3d(x+h,y-h,z+h); glVertex3d(x+h,y+h,z+h); glVertex3d(x-h,y+h,z+h);
    glVertex3d(x+h,y-h,z-h); glVertex3d(x-h,y-h,z-h); glVertex3d(x-h,y+h,z-h); glVertex3d(x+h,y+h,z-h);
    glVertex3d(x-h,y-h,z-h); glVertex3d(x-h,y-h,z+h); glVertex3d(x-h,y+h,z+h); glVertex3d(x-h,y+h,z-h);
    glVertex3d(x+h,y-h,z+h); glVertex3d(x+h,y-h,z-h); glVertex3d(x+h,y+h,z-h); glVertex3d(x+h,y+h,z+h);
    glVertex3d(x-h,y+h,z+h); glVertex3d(x+h,y+h,z+h); glVertex3d(x+h,y+h,z-h); glVertex3d(x-h,y+h,z-h);
    glVertex3d(x-h,y-h,z-h); glVertex3d(x+h,y-h,z-h); glVertex3d(x+h,y-h,z+h); glVertex3d(x-h,y-h,z+h);
    glEnd();
}

void WallMesh(double x, double z)
{
    glPushMatrix();
    glTranslated(x, 1.5, z);
    glScaled(1.0, 3.0, 1.0);
    CubeMesh(0.0, 0.0, 0.0, 1.0, 0.34f, 0.38f, 0.46f);
    glPopMatrix();
}

void DrawHudText(const std::string& text, int x, int y)
{
    glRasterPos2i(x, y);
    glListBase(fontBase - 32);
    glCallLists(static_cast<GLsizei>(text.size()), GL_UNSIGNED_BYTE, text.c_str());
}

void RenderHud()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.03f, 0.04f, 0.05f);
    glBegin(GL_QUADS);
    glVertex2i(18, 18); glVertex2i(278, 18); glVertex2i(278, 54); glVertex2i(18, 54);
    glEnd();
    glColor3f(0.82f, 0.12f, 0.10f);
    glBegin(GL_QUADS);
    glVertex2i(30, 30); glVertex2i(30 + 220 * playerHealth / 100, 30);
    glVertex2i(30 + 220 * playerHealth / 100, 43); glVertex2i(30, 43);
    glEnd();
    glColor3f(1.0f, 1.0f, 0.9f);
    DrawHudText("HP: " + std::to_string(playerHealth) + " / 100", 30, 28);
    DrawHudText("FPS: " + std::to_string(displayedFps), 18, 78);
    int remainingMedkits = 0;
    for (const Medkit& medkit : medkits)
        if (medkit.available) ++remainingMedkits;
    DrawHudText("MEDKITS: " + std::to_string(remainingMedkits), 18, 102);
    DrawHudText(fireTimer == 0.0 ? "HANDGUN: READY" : "HANDGUN: FIRING", width - 210, 30);

    glColor3f(1.0f, 0.92f, 0.62f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2i(width / 2 - 12, height / 2); glVertex2i(width / 2 - 4, height / 2);
    glVertex2i(width / 2 + 4, height / 2); glVertex2i(width / 2 + 12, height / 2);
    glVertex2i(width / 2, height / 2 - 12); glVertex2i(width / 2, height / 2 - 4);
    glVertex2i(width / 2, height / 2 + 4); glVertex2i(width / 2, height / 2 + 12);
    glEnd();
    DrawHudText("ESC - Quit", 18, height - 24);
    DrawHudText("WASD Move   Mouse Look   LMB Fire   SPACE Jump", 18, height - 48);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

void Update(double dt) {
    const double speed = 4.0 * dt;
    if (keys[VK_LEFT]) playerAngle -= 2.8 * dt;
    if (keys[VK_RIGHT]) playerAngle += 2.8 * dt;
    double moveX = 0.0, moveZ = 0.0;
    if (keys['W']) { moveX += cos(playerAngle)*speed; moveZ += sin(playerAngle)*speed; }
    if (keys['S']) { moveX -= cos(playerAngle)*speed; moveZ -= sin(playerAngle)*speed; }
    if (keys['A']) { moveX += sin(playerAngle)*speed; moveZ -= cos(playerAngle)*speed; }
    if (keys['D']) { moveX -= sin(playerAngle)*speed; moveZ += cos(playerAngle)*speed; }
    if (!PlayerPositionBlocked(playerX + moveX, playerZ)) playerX += moveX;
    if (!PlayerPositionBlocked(playerX, playerZ + moveZ)) playerZ += moveZ;
    if (keys[VK_SPACE] && jump == 0.0) jumpSpeed = 5.5;
    jumpSpeed -= 14.0 * dt;
    jump = std::max(0.0, jump + jumpSpeed * dt);
    if (jump == 0.0) jumpSpeed = 0.0;
    for (Cube& enemy : enemies) {
        if (!enemy.alive) continue;
        const double dx = playerX - enemy.x, dz = playerZ - enemy.z;
        const double distance = hypot(dx, dz);
        if (distance > 1.5) {
            const double enemySpeed = 0.65 * dt;
            if (!IsWall(enemy.x + dx / distance * enemySpeed, enemy.z)) enemy.x += dx / distance * enemySpeed;
            if (!IsWall(enemy.x, enemy.z + dz / distance * enemySpeed)) enemy.z += dz / distance * enemySpeed;
        }
    }
    for (Medkit& medkit : medkits) {
        if (medkit.available && playerHealth < 100 && hypot(medkit.x - playerX, medkit.z - playerZ) < 0.7) {
            playerHealth = std::min(100, playerHealth + 25);
            medkit.available = false;
        }
    }
    fireTimer = std::max(0.0, fireTimer - dt);
    fireFlash = std::max(0.0, fireFlash - dt);
    if (keys[VK_LBUTTON] && fireTimer == 0.0) {
        fireTimer = 0.2;
        fireFlash = 0.08;
        const double wallDistance = WallDistanceAlongAim();
        for (Cube& enemy : enemies) {
            const double dx = enemy.x-playerX, dz = enemy.z-playerZ, distance = hypot(dx,dz);
            const double difference = atan2(sin(atan2(dz,dx)-playerAngle), cos(atan2(dz,dx)-playerAngle));
            if (enemy.alive && distance < wallDistance && distance < 12.0 && abs(difference) < 0.1) { enemy.alive = false; break; }
        }
    }
}

void Render() {
    glClearColor(0.06f, 0.10f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(playerX, 1.65+jump, playerZ,
              playerX+cos(playerAngle), 1.65+jump, playerZ+sin(playerAngle), 0.0, 1.0, 0.0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, groundTexture);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS); glVertex3d(-20,0,-20); glVertex3d(40,0,-20); glVertex3d(40,0,40); glVertex3d(-20,0,40); glEnd();
    glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, skyTexture);
    glBegin(GL_QUADS); glVertex3d(-20,3,-20); glVertex3d(-20,3,40); glVertex3d(40,3,40); glVertex3d(40,3,-20); glEnd();
    glEnable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, brickTexture);
    for (int z=0; z<static_cast<int>(map.size()); ++z) for (int x=0; x<32; ++x)
        if (map[z][x]=='#') WallMesh(x+0.5, z+0.5);
    glDisable(GL_TEXTURE_2D);
    for (const Cube& enemy : enemies) if (enemy.alive) CubeMesh(enemy.x, 0.75, enemy.z, 0.8, 0.78f, 0.12f, 0.10f);
    for (const Medkit& medkit : medkits) if (medkit.available) {
        CubeMesh(medkit.x, 0.35, medkit.z, 0.55, 0.10f, 0.78f, 0.25f);
        CubeMesh(medkit.x, 0.35, medkit.z - 0.29, 0.22, 0.95f, 0.95f, 0.90f);
    }
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glLoadIdentity();
    CubeMesh(0.52, -0.42, -0.9, 0.28, 0.12f, 0.14f, 0.17f);
    CubeMesh(0.52, -0.58, -0.84, 0.18, 0.68f, 0.40f, 0.24f);
    if (fireFlash > 0.0) {
        glDisable(GL_DEPTH_TEST);
        glColor3f(1.0f, 0.72f, 0.08f);
        glBegin(GL_TRIANGLES);
        glVertex3d(0.52, -0.42, -1.08);
        glVertex3d(0.36, -0.30, -1.00);
        glVertex3d(0.45, -0.50, -1.00);
        glVertex3d(0.52, -0.42, -1.08);
        glVertex3d(0.68, -0.30, -1.00);
        glVertex3d(0.59, -0.50, -1.00);
        glEnd();
        glEnable(GL_DEPTH_TEST);
    }
    glPopMatrix();
    glEnable(GL_CULL_FACE);
    SwapBuffers(deviceContext);
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    RenderHud();
    switch (message) {
    case WM_KEYDOWN: if (wParam<256) keys[wParam]=true; if (wParam==VK_ESCAPE) PostQuitMessage(0); return 0;
    case WM_KEYUP: if (wParam<256) keys[wParam]=false; return 0;
    case WM_MOUSEMOVE:
        if (previousMouseX >= 0) playerAngle += (static_cast<int>(static_cast<short>(LOWORD(lParam))) - previousMouseX) * 0.004;
        previousMouseX = static_cast<int>(static_cast<short>(LOWORD(lParam)));
        return 0;
    case WM_LBUTTONDOWN: keys[VK_LBUTTON]=true; SetCapture(window); return 0;
    case WM_LBUTTONUP: keys[VK_LBUTTON]=false; ReleaseCapture(); return 0;
    case WM_SIZE:
        glViewport(0,0,LOWORD(lParam),HIWORD(lParam)); glMatrixMode(GL_PROJECTION); glLoadIdentity();
        gluPerspective(70.0, HIWORD(lParam) ? static_cast<double>(LOWORD(lParam))/HIWORD(lParam) : 1.0, 0.05, 100.0);
        glMatrixMode(GL_MODELVIEW); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProcW(window,message,wParam,lParam);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    WNDCLASSW windowClass = {};
    windowClass.hInstance=instance; windowClass.lpfnWndProc=WindowProcedure;
    windowClass.lpszClassName=L"OpenGL3DEngineWindow"; windowClass.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(IDC_ARROW));
    RegisterClassW(&windowClass);
    windowHandle=CreateWindowExW(0,windowClass.lpszClassName,L"2Dgames - OpenGL 3D Engine",WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT,CW_USEDEFAULT,width,height,nullptr,nullptr,instance,nullptr);
    if (!windowHandle) return 1;
    deviceContext=GetDC(windowHandle);
    PIXELFORMATDESCRIPTOR format={sizeof(PIXELFORMATDESCRIPTOR),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA,32,0,0,0,0,0,0,0,0,24,8,0,PFD_MAIN_PLANE,0,0,0,0};
    SetPixelFormat(deviceContext,ChoosePixelFormat(deviceContext,&format),&format);
    renderContext=wglCreateContext(deviceContext); wglMakeCurrent(deviceContext,renderContext);
    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glViewport(0,0,width,height); glMatrixMode(GL_PROJECTION);
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    const GLfloat textureSPlane[] = {1.0f, 0.0f, 0.0f, 0.0f};
    const GLfloat textureTPlane[] = {0.0f, 0.0f, 1.0f, 0.0f};
    glTexGenfv(GL_S, GL_OBJECT_PLANE, textureSPlane);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, textureTPlane);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    CreateBrickTexture();
    CreateEnvironmentTextures();
    CreateHudFont();
    gluPerspective(70.0,static_cast<double>(width)/height,0.05,100.0); ShowWindow(windowHandle,showCommand);
    fpsClock=std::chrono::steady_clock::now(); auto previous=fpsClock; MSG message={};
    while (message.message!=WM_QUIT) {
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        const auto now=std::chrono::steady_clock::now(); const double dt=std::min(0.1,std::chrono::duration<double>(now-previous).count()); previous=now;
        Update(dt); Render(); ++frameCount;
        if (std::chrono::duration<double>(now-fpsClock).count()>=1.0) {
            displayedFps=frameCount; frameCount=0; fpsClock=now; wchar_t title[128];
            wsprintfW(title,L"OpenGL 3D FPS | FPS: %d | HP: %d | WASD move, mouse look, Space jump, LMB fire",displayedFps,playerHealth);
            SetWindowTextW(windowHandle,title);
        }
    }
    wglMakeCurrent(nullptr,nullptr); wglDeleteContext(renderContext); ReleaseDC(windowHandle,deviceContext); return 0;
}
