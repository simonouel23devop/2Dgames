#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

namespace
{
constexpr int screenWidth = 960;
constexpr int screenHeight = 540;
constexpr double pi = 3.14159265358979323846;

const std::array<std::string, 16> level = {
    "################",
    "#..............#",
    "#..####........#",
    "#..#..#........#",
    "#..#..#....###.#",
    "#..####....#...#",
    "#............#.#",
    "#....###.....#.#",
    "#............#.#",
    "#..##........#.#",
    "#..#.........#.#",
    "#..#.........#.#",
    "#............#.#",
    "#......####....#",
    "#..............#",
    "################"};

struct Player
{
    double x = 2.5;
    double y = 2.5;
    double angle = 0.0;
    double height = 0.0;
    double verticalVelocity = 0.0;
};

struct Enemy
{
    double x;
    double y;
    double health;
    double attackCooldown;
    bool alive;
};

Player player;
std::array<Enemy, 3> enemies = {{{8.5, 2.5, 3.0, 0.0, true},
                                 {11.5, 7.5, 3.0, 0.0, true},
                                 {3.5, 12.5, 3.0, 0.0, true}}};
bool keys[256] = {};
bool jumpRequested = false;
bool fireRequested = false;
HBITMAP backBuffer = nullptr;
HDC backDc = nullptr;
std::uint32_t* backPixels = nullptr;
int frameRate = 0;
double shotCooldown = 0.0;
double muzzleFlashTime = 0.0;
std::array<double, screenWidth> depthBuffer = {};
int playerHealth = 100;

bool IsWall(double x, double y)
{
    const int mapX = static_cast<int>(x);
    const int mapY = static_cast<int>(y);
    return mapY < 0 || mapY >= static_cast<int>(level.size()) ||
           mapX < 0 || mapX >= static_cast<int>(level[mapY].size()) ||
           level[mapY][mapX] == '#';
}

void HitEnemyInSight()
{
    Enemy* closestEnemy = nullptr;
    double closestDistance = 12.0;
    for (Enemy& enemy : enemies)
    {
        if (!enemy.alive)
            continue;

        const double distanceX = enemy.x - player.x;
        const double distanceY = enemy.y - player.y;
        const double distance = std::hypot(distanceX, distanceY);
        const double enemyAngle = std::atan2(distanceY, distanceX);
        const double angleDifference = std::atan2(std::sin(enemyAngle - player.angle),
                                                  std::cos(enemyAngle - player.angle));
        if (std::abs(angleDifference) < 0.07 && distance < closestDistance)
        {
            closestDistance = distance;
            closestEnemy = &enemy;
        }
    }
    if (closestEnemy != nullptr)
    {
        closestEnemy->health -= 1.0;
        if (closestEnemy->health <= 0.0)
            closestEnemy->alive = false;
    }
}

void UpdateEnemies(double deltaSeconds)
{
    constexpr double enemySpeed = 0.8;
    for (Enemy& enemy : enemies)
    {
        if (!enemy.alive)
            continue;

        enemy.attackCooldown = std::max(0.0, enemy.attackCooldown - deltaSeconds);
        const double distanceX = player.x - enemy.x;
        const double distanceY = player.y - enemy.y;
        const double distance = std::hypot(distanceX, distanceY);
        if (distance > 1.25)
        {
            const double directionX = distanceX / distance;
            const double directionY = distanceY / distance;
            const double moveDistance = enemySpeed * deltaSeconds;
            if (!IsWall(enemy.x + directionX * moveDistance, enemy.y))
                enemy.x += directionX * moveDistance;
            if (!IsWall(enemy.x, enemy.y + directionY * moveDistance))
                enemy.y += directionY * moveDistance;
        }
        else if (enemy.attackCooldown == 0.0)
        {
            playerHealth = std::max(0, playerHealth - 8);
            enemy.attackCooldown = 0.75;
        }
    }

    if (playerHealth == 0)
    {
        player.x = 2.5;
        player.y = 2.5;
        playerHealth = 100;
    }
}

void UpdatePlayer(double deltaSeconds)
{
    const double moveSpeed = 4 * deltaSeconds;
    const double turnSpeed = 3 * deltaSeconds;
    constexpr double jumpVelocity = 5.5;
    constexpr double gravity = 14.0;
    double moveX = 0.0;
    double moveY = 0.0;

    shotCooldown = std::max(0.0, shotCooldown - deltaSeconds);
    muzzleFlashTime = std::max(0.0, muzzleFlashTime - deltaSeconds);
    if (fireRequested && shotCooldown == 0.0)
    {
        shotCooldown = 0.18;
        muzzleFlashTime = 0.08;
        HitEnemyInSight();
    }
    fireRequested = false;

    if (jumpRequested && player.height == 0.0)
        player.verticalVelocity = jumpVelocity;
    jumpRequested = false;

    player.verticalVelocity -= gravity * deltaSeconds;
    player.height += player.verticalVelocity * deltaSeconds;
    if (player.height <= 0.0)
    {
        player.height = 0.0;
        player.verticalVelocity = 0.0;
    }

    if (keys['W'])
    {
        moveX += std::cos(player.angle) * moveSpeed;
        moveY += std::sin(player.angle) * moveSpeed;
    }
    if (keys['S'])
    {
        moveX -= std::cos(player.angle) * moveSpeed;
        moveY -= std::sin(player.angle) * moveSpeed;
    }
    if (keys['A'])
    {
        moveX += std::cos(player.angle - pi / 2.0) * moveSpeed;
        moveY += std::sin(player.angle - pi / 2.0) * moveSpeed;
    }
    if (keys['D'])
    {
        moveX += std::cos(player.angle + pi / 2.0) * moveSpeed;
        moveY += std::sin(player.angle + pi / 2.0) * moveSpeed;
    }
    if (keys[VK_LEFT])
        player.angle -= turnSpeed;
    if (keys[VK_RIGHT])
        player.angle += turnSpeed;

    if (!IsWall(player.x + moveX, player.y))
        player.x += moveX;
    if (!IsWall(player.x, player.y + moveY))
        player.y += moveY;
}

void RenderScene()
{
    const std::uint32_t skyColor = static_cast<std::uint32_t>(RGB(24, 34, 54));
    const std::uint32_t floorColor = static_cast<std::uint32_t>(RGB(52, 49, 48));
    std::fill(backPixels, backPixels + screenWidth * (screenHeight / 2), skyColor);
    std::fill(backPixels + screenWidth * (screenHeight / 2),
              backPixels + screenWidth * screenHeight, floorColor);

    constexpr double fieldOfView = pi / 3.0;
    const int cameraOffset = static_cast<int>(player.height * 55.0);
    for (int column = 0; column < screenWidth; ++column)
    {
        const double cameraAngle = player.angle - fieldOfView / 2.0 +
                                   fieldOfView * column / screenWidth;
        const double rayX = std::cos(cameraAngle);
        const double rayY = std::sin(cameraAngle);
        int mapX = static_cast<int>(player.x);
        int mapY = static_cast<int>(player.y);
        const double deltaDistanceX = std::abs(1.0 / rayX);
        const double deltaDistanceY = std::abs(1.0 / rayY);
        const int stepX = rayX < 0.0 ? -1 : 1;
        const int stepY = rayY < 0.0 ? -1 : 1;
        double sideDistanceX = rayX < 0.0 ? (player.x - mapX) * deltaDistanceX
                                          : (mapX + 1.0 - player.x) * deltaDistanceX;
        double sideDistanceY = rayY < 0.0 ? (player.y - mapY) * deltaDistanceY
                                          : (mapY + 1.0 - player.y) * deltaDistanceY;
        int wallSide = 0;

        while (!IsWall(mapX + 0.5, mapY + 0.5))
        {
            if (sideDistanceX < sideDistanceY)
            {
                sideDistanceX += deltaDistanceX;
                mapX += stepX;
                wallSide = 0;
            }
            else
            {
                sideDistanceY += deltaDistanceY;
                mapY += stepY;
                wallSide = 1;
            }
        }

        const double wallDistance = wallSide == 0 ? sideDistanceX - deltaDistanceX
                                                  : sideDistanceY - deltaDistanceY;
        const double correctedDistance = std::max(0.1, wallDistance);
        const int wallHeight = static_cast<int>(screenHeight / correctedDistance);
        const int wallTop = std::max(0, screenHeight / 2 - wallHeight / 2 + cameraOffset);
        const int wallBottom = std::min(screenHeight, screenHeight / 2 + wallHeight / 2 + cameraOffset);
        const int shade = std::max(35, 220 - static_cast<int>(correctedDistance * 15.0));
        const bool alternateWall = ((mapX + mapY + wallSide) % 2) == 0;
        const COLORREF wallColor = alternateWall ? RGB(shade, shade / 2, shade / 3)
                                                 : RGB(shade / 2, shade * 2 / 3, shade / 3);

        for (int y = wallTop; y < wallBottom; ++y)
            backPixels[y * screenWidth + column] = static_cast<std::uint32_t>(wallColor);
        depthBuffer[column] = correctedDistance;
    }

    for (const Enemy& enemy : enemies)
    {
        if (!enemy.alive)
            continue;

        const double distanceX = enemy.x - player.x;
        const double distanceY = enemy.y - player.y;
        const double distance = std::hypot(distanceX, distanceY);
        const double enemyAngle = std::atan2(distanceY, distanceX);
        const double angleDifference = std::atan2(std::sin(enemyAngle - player.angle),
                                                  std::cos(enemyAngle - player.angle));
        if (std::abs(angleDifference) > fieldOfView / 2.0 + 0.15)
            continue;

        const double projectedDistance = std::max(0.1, distance * std::cos(angleDifference));
        const int enemyCenterX = static_cast<int>(screenWidth / 2.0 +
                                                   std::tan(angleDifference) /
                                                       std::tan(fieldOfView / 2.0) * (screenWidth / 2.0));
        const int cubeSize = std::max(8, static_cast<int>(screenHeight * 0.55 / projectedDistance));
        const int cubeTop = std::max(0, screenHeight / 2 - cubeSize / 2 + cameraOffset);
        const int cubeBottom = std::min(screenHeight, screenHeight / 2 + cubeSize / 2 + cameraOffset);
        const int cubeLeft = std::max(0, enemyCenterX - cubeSize / 2);
        const int cubeRight = std::min(screenWidth, enemyCenterX + cubeSize / 2);
        const int shade = std::max(70, 235 - static_cast<int>(projectedDistance * 16.0));

        for (int column = cubeLeft; column < cubeRight; ++column)
        {
            if (projectedDistance >= depthBuffer[column])
                continue;
            const bool sideFace = column > enemyCenterX + cubeSize / 5;
            const COLORREF cubeColor = sideFace ? RGB(shade / 2, shade / 2, shade)
                                                : RGB(shade, shade / 4, shade / 5);
            for (int row = cubeTop; row < cubeBottom; ++row)
                backPixels[row * screenWidth + column] = static_cast<std::uint32_t>(cubeColor);
        }
    }

    const int centerX = screenWidth / 2;
    const int centerY = screenHeight / 2;
    HPEN crosshairPen = CreatePen(PS_SOLID, 2, RGB(240, 240, 220));
    HPEN oldPen = static_cast<HPEN>(SelectObject(backDc, crosshairPen));
    MoveToEx(backDc, centerX - 8, centerY, nullptr);
    LineTo(backDc, centerX + 9, centerY);
    MoveToEx(backDc, centerX, centerY - 8, nullptr);
    LineTo(backDc, centerX, centerY + 9);
    SelectObject(backDc, oldPen);
    DeleteObject(crosshairPen);

    const int weaponX = centerX + 190;
    POINT hand[] = {{weaponX - 48, screenHeight}, {weaponX - 41, screenHeight - 82},
                    {weaponX - 20, screenHeight - 112}, {weaponX + 24, screenHeight - 108},
                    {weaponX + 48, screenHeight - 62}, {weaponX + 56, screenHeight},
                    {weaponX - 48, screenHeight}};
    POINT handgunGrip[] = {{weaponX - 27, screenHeight}, {weaponX - 19, screenHeight - 84},
                           {weaponX + 19, screenHeight - 84}, {weaponX + 27, screenHeight},
                           {weaponX - 27, screenHeight}};
    RECT handgunSlide = {weaponX - 51, screenHeight - 137, weaponX + 51, screenHeight - 101};
    RECT handgunBarrel = {weaponX - 14, screenHeight - 173, weaponX + 14, screenHeight - 136};
    HBRUSH handBrush = CreateSolidBrush(RGB(177, 112, 77));
    HBRUSH handgunBrush = CreateSolidBrush(RGB(34, 38, 43));
    HBRUSH gripBrush = CreateSolidBrush(RGB(21, 24, 28));
    HPEN handgunPen = CreatePen(PS_SOLID, 2, RGB(8, 10, 12));
    oldPen = static_cast<HPEN>(SelectObject(backDc, handgunPen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(backDc, handBrush));
    Polygon(backDc, hand, static_cast<int>(std::size(hand)));
    SelectObject(backDc, handgunBrush);
    Rectangle(backDc, handgunSlide.left, handgunSlide.top, handgunSlide.right, handgunSlide.bottom);
    Rectangle(backDc, handgunBarrel.left, handgunBarrel.top, handgunBarrel.right, handgunBarrel.bottom);
    SelectObject(backDc, gripBrush);
    Polygon(backDc, handgunGrip, static_cast<int>(std::size(handgunGrip)));
    SelectObject(backDc, oldBrush);
    SelectObject(backDc, oldPen);
    DeleteObject(handBrush);
    DeleteObject(handgunBrush);
    DeleteObject(gripBrush);
    DeleteObject(handgunPen);

    if (muzzleFlashTime > 0.0)
    {
        POINT flash[] = {{weaponX, screenHeight - 176},
                 {weaponX - 30, screenHeight - 207},
                 {weaponX - 11, screenHeight - 172},
                 {weaponX - 18, screenHeight - 151},
                 {weaponX, screenHeight - 166},
                 {weaponX + 18, screenHeight - 151},
                 {weaponX + 11, screenHeight - 172},
                 {weaponX + 30, screenHeight - 207}};
        HBRUSH flashBrush = CreateSolidBrush(RGB(255, 211, 66));
        HBRUSH previousBrush = static_cast<HBRUSH>(SelectObject(backDc, flashBrush));
        Polygon(backDc, flash, static_cast<int>(std::size(flash)));
        SelectObject(backDc, previousBrush);
        DeleteObject(flashBrush);

        HPEN tracerPen = CreatePen(PS_SOLID, 2, RGB(255, 220, 95));
        oldPen = static_cast<HPEN>(SelectObject(backDc, tracerPen));
        MoveToEx(backDc, weaponX, screenHeight - 174, nullptr);
        LineTo(backDc, centerX, centerY + 12);
        SelectObject(backDc, oldPen);
        DeleteObject(tracerPen);
    }
}

void DrawHud(HDC windowDc)
{
    SetBkMode(windowDc, TRANSPARENT);
    SetTextColor(windowDc, RGB(238, 232, 204));
    HFONT hudFont = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
    HFONT oldFont = static_cast<HFONT>(SelectObject(windowDc, hudFont));
    std::wstringstream status;
        status << L"FPS " << frameRate << L"    HP " << playerHealth
            << L"    W A S D move    Arrows look    Space jump    LMB fire    ESC quit";
    const std::wstring statusText = status.str();
    TextOutW(windowDc, 18, 16, statusText.c_str(), static_cast<int>(statusText.size()));
    SelectObject(windowDc, oldFont);
    DeleteObject(hudFont);
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam < 256)
            keys[wParam] = true;
        if (wParam == VK_SPACE && (lParam & (1 << 30)) == 0)
            jumpRequested = true;
        if (wParam == VK_ESCAPE)
            PostQuitMessage(0);
        return 0;
    case WM_KEYUP:
        if (wParam < 256)
            keys[wParam] = false;
        return 0;
    case WM_LBUTTONDOWN:
        fireRequested = true;
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC windowDc = BeginPaint(window, &paint);
        BitBlt(windowDc, 0, 0, screenWidth, screenHeight, backDc, 0, 0, SRCCOPY);
        DrawHud(windowDc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    WNDCLASSW windowClass = {};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = L"FpsRaycasterWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&windowClass);

    RECT windowRect = {0, 0, screenWidth, screenHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"2Dgames - First Person Raycaster",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  windowRect.right - windowRect.left,
                                  windowRect.bottom - windowRect.top,
                                  nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;

    HDC windowDc = GetDC(window);
    backDc = CreateCompatibleDC(windowDc);
    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = screenWidth;
    bitmapInfo.bmiHeader.biHeight = -screenHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    backBuffer = CreateDIBSection(windowDc, &bitmapInfo, DIB_RGB_COLORS,
                                  reinterpret_cast<void**>(&backPixels), nullptr, 0);
    SelectObject(backDc, backBuffer);
    ReleaseDC(window, windowDc);

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    auto previousTime = std::chrono::steady_clock::now();
    auto fpsTime = previousTime;
    int frameCount = 0;
    MSG message = {};
    while (message.message != WM_QUIT)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const double deltaSeconds = std::min(0.1, std::chrono::duration<double>(currentTime - previousTime).count());
        previousTime = currentTime;
        UpdatePlayer(deltaSeconds);
        UpdateEnemies(deltaSeconds);
        RenderScene();
        InvalidateRect(window, nullptr, FALSE);

        ++frameCount;
        if (std::chrono::duration<double>(currentTime - fpsTime).count() >= 1.0)
        {
            frameRate = frameCount;
            frameCount = 0;
            fpsTime = currentTime;
        }
    }

    DeleteObject(backBuffer);
    DeleteDC(backDc);
    return 0;
}