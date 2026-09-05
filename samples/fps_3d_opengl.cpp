#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#pragma comment(lib, "winmm.lib")

namespace {
// Fixed viewport dimensions keep the legacy OpenGL projection predictable.
constexpr int width = 1280; // Window width and height are fixed to avoid resizing complications with the OpenGL context.
constexpr int height = 720; 
constexpr double pi = 3.14159265358979323846;
const std::array<const char*, 24> map = { // Define the game level layout
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
// World entities use compact state because this sample has a fixed-size level.
struct Cube { double x, z; bool alive; };
struct Medkit { double x, z; bool available; };
// Projectiles store position and normalized direction for simple frame-based motion.
struct EnemyBullet { double x, y, z, dx, dy, dz; bool active; };
struct PlayerTrace { double x, y, z, dx, dy, dz, lifetime; bool active; };
enum class Weapon { Handgun, Shotgun };
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
std::array<EnemyBullet, 24> enemyBullets = {};
std::array<PlayerTrace, 8> playerTraces = {};
double playerX = 2.5, playerZ = 2.5, playerAngle = 0.0, playerPitch = 0.0, jump = 0.0, jumpSpeed = 0.0, fireTimer = 0.0, fireFlash = 0.0;
double weaponBobTime = 0.0, weaponBobAmount = 0.0, weaponRecoil = 0.0, cameraRecoil = 0.0, breathingTime = 0.0;
double crouchAmount = 0.0;
Weapon currentWeapon = Weapon::Handgun;
double enemyFireTimer = 0.8;
bool paused = false;
int frameCount = 0, displayedFps = 0;
std::chrono::steady_clock::time_point fpsClock;
int previousMouseX = -1, previousMouseY = -1;
bool mouseLookCaptured = false;
bool ignoreMouseMove = false;
int playerHealth = 100;
GLuint brickTexture = 0;
GLuint groundTexture = 0;
GLuint skyTexture = 0;
GLuint fontBase = 0;
constexpr int soundSampleRate = 22050;
constexpr int handgunSoundSamples = 2205;
constexpr int shotgunSoundSamples = 3969;
constexpr int musicSamples = soundSampleRate * 16;
std::array<std::uint8_t, 44 + handgunSoundSamples * 2> handgunSound = {};
std::array<std::uint8_t, 44 + shotgunSoundSamples * 2> shotgunSound = {};
std::array<std::int16_t, musicSamples> rockMusic = {};
HWAVEOUT musicOutput = nullptr;
WAVEHDR musicHeader = {};

const char* WeaponName()
{
    return currentWeapon == Weapon::Handgun ? "HANDGUN" : "SHOTGUN";
}

const char* WeaponSlot()
{
    return currentWeapon == Weapon::Handgun ? "1/2" : "2/2";
}

const char* FacingDirection()
{
    int direction = static_cast<int>(std::floor((playerAngle + pi / 4.0) / (pi / 2.0))) % 4;
    if (direction < 0) direction += 4;
    constexpr const char* directions[] = {"E", "S", "W", "N"};
    return directions[direction];
}

void WriteSoundValue(std::uint8_t* destination, std::uint32_t value, int byteCount)
{
    for (int byte = 0; byte < byteCount; ++byte)
        destination[byte] = static_cast<std::uint8_t>((value >> (byte * 8)) & 0xff);
}

template <std::size_t soundSize>
void CreateWeaponSound(std::array<std::uint8_t, soundSize>& sound, bool shotgun)
{
    constexpr std::size_t sampleCount = (soundSize - 44) / 2;
    const std::uint32_t dataSize = sampleCount * 2;
    const char riff[] = "RIFF", wave[] = "WAVE", format[] = "fmt ", data[] = "data";
    std::copy(riff, riff + 4, sound.begin());
    WriteSoundValue(sound.data() + 4, sound.size() - 8, 4);
    std::copy(wave, wave + 4, sound.begin() + 8);
    std::copy(format, format + 4, sound.begin() + 12);
    WriteSoundValue(sound.data() + 16, 16, 4);
    WriteSoundValue(sound.data() + 20, 1, 2);
    WriteSoundValue(sound.data() + 22, 1, 2);
    WriteSoundValue(sound.data() + 24, soundSampleRate, 4);
    WriteSoundValue(sound.data() + 28, soundSampleRate * 2, 4);
    WriteSoundValue(sound.data() + 32, 2, 2);
    WriteSoundValue(sound.data() + 34, 16, 2);
    std::copy(data, data + 4, sound.begin() + 36);
    WriteSoundValue(sound.data() + 40, dataSize, 4);

    for (std::size_t sample = 0; sample < sampleCount; ++sample) {
        const double time = static_cast<double>(sample) / soundSampleRate;
        const double envelope = shotgun ? std::exp(-18.0 * time) : std::exp(-32.0 * time);
        const std::uint32_t noiseBits = static_cast<std::uint32_t>(sample * 1103515245u + 12345u);
        const double noise = (static_cast<double>(noiseBits & 0xffff) / 32768.0) - 1.0;
        const double tone = shotgun ? std::sin(2.0 * pi * 72.0 * time) : std::sin(2.0 * pi * 180.0 * time);
        const double volume = shotgun ? (noise * 0.9 + tone * 0.35) : (noise * 0.35 + tone * 0.7);
        const auto pcm = static_cast<std::int16_t>(std::clamp(volume * envelope * 28000.0, -32768.0, 32767.0));
        WriteSoundValue(sound.data() + 44 + sample * 2, static_cast<std::uint16_t>(pcm), 2);
    }
}

void CreateWeaponSounds()
{
    CreateWeaponSound(handgunSound, false);
    CreateWeaponSound(shotgunSound, true);
}

void PlayWeaponSound()
{
    if (currentWeapon == Weapon::Handgun)
        PlaySoundA(reinterpret_cast<LPCSTR>(handgunSound.data()), nullptr, SND_ASYNC | SND_MEMORY | SND_NODEFAULT);
    else
        PlaySoundA(reinterpret_cast<LPCSTR>(shotgunSound.data()), nullptr, SND_ASYNC | SND_MEMORY | SND_NODEFAULT);
}

void CALLBACK RockMusicCallback(HWAVEOUT output, UINT message, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    if (message != WOM_DONE || !musicOutput) return;
    waveOutWrite(output, &musicHeader, sizeof(musicHeader));
}

void CreateRockMusic() // Procedurally generate a simple rock music track with bass, kick, snare, and guitar.
{
    constexpr double secondsPerBeat = 0.5;
    constexpr double roots[] = {110.0, 130.81, 98.0, 87.31};
    for (int sample = 0; sample < musicSamples; ++sample) {
        const double time = static_cast<double>(sample) / soundSampleRate;
        const int beat = static_cast<int>(time / secondsPerBeat) % 16;
        const int bar = (beat / 4) % 4;
        const double beatTime = fmod(time, secondsPerBeat);
        const std::uint32_t noiseBits = static_cast<std::uint32_t>(sample * 1664525u + 1013904223u);
        const double noise = (static_cast<double>(noiseBits & 0xffff) / 32768.0) - 1.0;
        const double root = roots[bar];
        const double bassEnvelope = std::exp(-3.0 * beatTime);
        const double bass = sin(2.0 * pi * root * time) * bassEnvelope * 0.24;
        const bool kickHit = beat % 4 == 0 || beat % 4 == 2;
        const double kickEnvelope = kickHit && beatTime < 0.15
            ? std::exp(-26.0 * beatTime) * sin(2.0 * pi * (85.0 - beatTime * 220.0) * beatTime) * 0.48 : 0.0;
        const bool snareHit = beat % 4 == 1 || beat % 4 == 3;
        const double snareSound = snareHit && beatTime > 0.16 && beatTime < 0.34
            ? noise * 0.22 * std::exp(-18.0 * (beatTime - 0.16)) : 0.0;
        const double hatTime = fmod(time, secondsPerBeat / 2.0);
        const bool hatHit = hatTime < 0.045;
        const double hiHat = hatHit ? noise * 0.07 * std::exp(-55.0 * hatTime) : 0.0;
        const double guitarPhase = 2.0 * pi * root * 2.0 * time;
        const double guitarWave = sin(guitarPhase) + 0.45 * sin(guitarPhase * 2.0) + 0.2 * sin(guitarPhase * 3.0);
        const double guitar = std::clamp(guitarWave * 0.07, -0.09, 0.09) * std::exp(-2.0 * beatTime);
        const double lead = (beat % 4 == 3) ? sin(2.0 * pi * root * 4.0 * time) * 0.045 * std::exp(-5.0 * beatTime) : 0.0;
        const double mix = bass + kickEnvelope + snareSound + hiHat + guitar + lead;
        rockMusic[sample] = static_cast<std::int16_t>(std::clamp(mix * 28000.0, -32768.0, 32767.0));
    }
}

void StartRockMusic()
{
    WAVEFORMATEX format = {WAVE_FORMAT_PCM, 1, soundSampleRate, soundSampleRate * 2, 2, 16, 0};
    if (waveOutOpen(&musicOutput, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(RockMusicCallback), 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
        return;
    musicHeader.lpData = reinterpret_cast<LPSTR>(rockMusic.data());
    musicHeader.dwBufferLength = static_cast<DWORD>(rockMusic.size() * sizeof(std::int16_t));
    waveOutPrepareHeader(musicOutput, &musicHeader, sizeof(musicHeader));
    waveOutWrite(musicOutput, &musicHeader, sizeof(musicHeader));
}

void StopRockMusic()
{
    if (!musicOutput) return;
    HWAVEOUT output = musicOutput;
    musicOutput = nullptr;
    waveOutReset(output);
    waveOutUnprepareHeader(output, &musicHeader, sizeof(musicHeader));
    waveOutClose(output);
}

void SetMouseLookCaptured(HWND window, bool captured);

void SetMusicPaused(bool shouldPause)
{
    if (!musicOutput) return;
    if (shouldPause) waveOutPause(musicOutput);
    else waveOutRestart(musicOutput);
}

void SetGamePaused(HWND window, bool shouldPause)
{
    paused = shouldPause;
    SetMouseLookCaptured(window, !paused);
    SetMusicPaused(paused);
}

void CenterMouse(HWND window)
{
    RECT clientRect = {};
    GetClientRect(window, &clientRect);
    POINT center = {(clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2};
    ClientToScreen(window, &center);
    ignoreMouseMove = true;
    SetCursorPos(center.x, center.y);
}

void SetMouseLookCaptured(HWND window, bool captured)
{
    if (mouseLookCaptured == captured) return;
    mouseLookCaptured = captured;
    previousMouseX = -1;
    previousMouseY = -1;
    if (captured) {
        SetCapture(window);
        ShowCursor(FALSE);
        CenterMouse(window);
    } else {
        ReleaseCapture();
        ShowCursor(TRUE);
        ignoreMouseMove = false;
    }
}

bool IsWall(double x, double z) {
    const int mapX = static_cast<int>(x), mapZ = static_cast<int>(z);
    return mapZ < 0 || mapZ >= static_cast<int>(map.size()) || mapX < 0 || mapX >= 32 || map[mapZ][mapX] == '#';
}

// Shared footprint collision prevents actors from entering walls with their corners.
bool PositionBlocked(double x, double z, double radius)
{
    return IsWall(x - radius, z - radius) ||
           IsWall(x + radius, z - radius) ||
           IsWall(x - radius, z + radius) ||
           IsWall(x + radius, z + radius);
}

bool PlayerPositionBlocked(double x, double z)
{
    return PositionBlocked(x, z, 0.28);
}

bool EnemyPositionBlocked(double x, double z) // 
{
    return PositionBlocked(x, z, 0.4);
}

double WallDistanceAlongAim()
{
    for (double distance = 0.2; distance < 20.0; distance += 0.05)
        if (IsWall(playerX + cos(playerAngle) * distance, playerZ + sin(playerAngle) * distance))
            return distance;
    return 20.0;
}

// Enemy shots only start when the map does not obstruct the target.
bool ClearLineOfSight(double startX, double startZ, double endX, double endZ)
{
    const double distance = hypot(endX - startX, endZ - startZ);
    const int steps = static_cast<int>(distance / 0.2);
    for (int step = 1; step < steps; ++step) {
        const double progress = static_cast<double>(step) / steps;
        if (IsWall(startX + (endX - startX) * progress, startZ + (endZ - startZ) * progress)) return false;
    }
    return true;
}

void SpawnPlayerTrace()
{
    const double horizontalAim = cos(playerPitch);
    const double directionX = cos(playerAngle) * horizontalAim;
    const double directionY = sin(playerPitch);
    const double directionZ = sin(playerAngle) * horizontalAim;
    for (PlayerTrace& trace : playerTraces) {
        if (trace.active) continue;
        trace = {playerX + directionX * 0.2, 1.65 + jump + directionY * 0.2,
                 playerZ + directionZ * 0.2, directionX, directionY, directionZ, 0.12, true};
        break;
    }
}

void CreateBrickTexture()
{
    // Build textures once during startup; uploading pixels inside Render() would stall the GPU.
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
    std::array<std::uint32_t, 64 * 64> groundPixels = {}; // Generate a simple concrete texture with seams every 32 pixels.
    std::array<std::uint32_t, 64 * 64> skyPixels = {};
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            groundPixels[y * 64 + x] = 0xffffffff;
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

void RenderHandgun()
{
    // Layered parts give the first-person handgun a readable silhouette at low polygon cost.
    glColor3f(0.08f, 0.09f, 0.11f);
    CubeMesh(0.52, -0.40, -1.02, 0.34, 0.08f, 0.09f, 0.11f);
    CubeMesh(0.52, -0.39, -1.23, 0.12, 0.08f, 0.09f, 0.11f);
    glColor3f(0.32f, 0.36f, 0.42f);
    CubeMesh(0.52, -0.34, -0.98, 0.22, 0.32f, 0.36f, 0.42f);
    glColor3f(0.16f, 0.18f, 0.22f);
    CubeMesh(0.52, -0.58, -0.82, 0.20, 0.16f, 0.18f, 0.22f);
    glColor3f(0.45f, 0.24f, 0.12f);
    CubeMesh(0.52, -0.61, -0.78, 0.24, 0.42f, 0.22f, 0.12f);
    glColor3f(0.72f, 0.76f, 0.82f);
    CubeMesh(0.52, -0.25, -1.08, 0.06, 0.70f, 0.72f, 0.78f);
    glColor3f(0.05f, 0.06f, 0.07f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    glVertex3d(0.43, -0.49, -0.87); glVertex3d(0.43, -0.55, -0.87);
    glVertex3d(0.61, -0.55, -0.87); glVertex3d(0.61, -0.49, -0.87);
    glEnd();
}

void RenderShotgun()
{
    // A longer barrel and wood-toned pump make the shotgun distinct from the handgun.
    glColor3f(0.20f, 0.12f, 0.08f);
    CubeMesh(0.52, -0.46, -0.70, 0.28, 0.24f, 0.13f, 0.07f);
    glColor3f(0.12f, 0.13f, 0.15f);
    CubeMesh(0.52, -0.36, -0.94, 0.30, 0.12f, 0.13f, 0.15f);
    CubeMesh(0.47, -0.30, -1.20, 0.11, 0.12f, 0.13f, 0.15f);
    CubeMesh(0.57, -0.30, -1.20, 0.11, 0.12f, 0.13f, 0.15f);
    glColor3f(0.38f, 0.22f, 0.12f);
    CubeMesh(0.52, -0.48, -1.02, 0.24, 0.44f, 0.25f, 0.13f);
    glColor3f(0.72f, 0.76f, 0.82f);
    CubeMesh(0.52, -0.23, -1.12, 0.06, 0.70f, 0.72f, 0.78f);
}

void RenderMinimap() // Draw a player-centered circular radar in the corner of the screen.
{
    constexpr double radarScale = 7.0;
    constexpr int radarRadius = 74;
    constexpr int segmentCount = 40;
    const int centerX = width - radarRadius - 24;
    const int centerY = 154;
    // Bind the top-down camera to the player so the player stays centered as the world moves.
    const double topViewCameraX = playerX;
    const double topViewCameraZ = playerZ;
    const auto insideRadar = [centerX, centerY, radarRadius](double x, double y) {
        const double dx = x - centerX, dy = y - centerY;
        return dx * dx + dy * dy <= radarRadius * radarRadius;
    };

    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(centerX, centerY);
    for (int segment = 0; segment <= segmentCount; ++segment) {
        const double angle = 2.0 * pi * segment / segmentCount;
        glVertex2i(centerX + static_cast<int>(cos(angle) * radarRadius),
                   centerY + static_cast<int>(sin(angle) * radarRadius));
    }
    glEnd();

    glColor3f(0.24f, 0.28f, 0.34f);
    glBegin(GL_QUADS);
    for (int z = 0; z < static_cast<int>(map.size()); ++z) {
        for (int x = 0; x < 32; ++x) {
            if (map[z][x] != '#') continue;
            const double worldX = x + 0.5, worldZ = z + 0.5;
            const int wallX = centerX + static_cast<int>((worldX - topViewCameraX) * radarScale);
            const int wallY = centerY + static_cast<int>((worldZ - topViewCameraZ) * radarScale);
            if (!insideRadar(wallX, wallY)) continue;
            glVertex2i(wallX - 3, wallY - 3); glVertex2i(wallX + 3, wallY - 3);
            glVertex2i(wallX + 3, wallY + 3); glVertex2i(wallX - 3, wallY + 3);
        }
    }
    glEnd();

    glColor3f(0.86f, 0.12f, 0.10f);
    glBegin(GL_QUADS);
    for (const Cube& enemy : enemies) {
        if (!enemy.alive) continue;
        const int enemyX = centerX + static_cast<int>((enemy.x - topViewCameraX) * radarScale);
        const int enemyY = centerY + static_cast<int>((enemy.z - topViewCameraZ) * radarScale);
        if (!insideRadar(enemyX, enemyY)) continue;
        glVertex2i(enemyX - 3, enemyY - 3); glVertex2i(enemyX + 3, enemyY - 3);
        glVertex2i(enemyX + 3, enemyY + 3); glVertex2i(enemyX - 3, enemyY + 3);
    }
    glEnd();

    glColor3f(0.18f, 0.82f, 0.30f);
    glBegin(GL_QUADS);
    for (const Medkit& medkit : medkits) {
        if (!medkit.available) continue;
        const int medkitX = centerX + static_cast<int>((medkit.x - topViewCameraX) * radarScale);
        const int medkitY = centerY + static_cast<int>((medkit.z - topViewCameraZ) * radarScale);
        if (!insideRadar(medkitX, medkitY)) continue;
        glVertex2i(medkitX - 2, medkitY - 2); glVertex2i(medkitX + 2, medkitY - 2);
        glVertex2i(medkitX + 2, medkitY + 2); glVertex2i(medkitX - 2, medkitY + 2);
    }
    glEnd();

    glColor3f(1.0f, 0.86f, 0.18f);
    glBegin(GL_TRIANGLES);
    glVertex2i(centerX + static_cast<int>(cos(playerAngle) * 10.0), centerY + static_cast<int>(sin(playerAngle) * 10.0));
    glVertex2i(centerX + static_cast<int>(cos(playerAngle + 2.5) * 7.0), centerY + static_cast<int>(sin(playerAngle + 2.5) * 7.0));
    glVertex2i(centerX + static_cast<int>(cos(playerAngle - 2.5) * 7.0), centerY + static_cast<int>(sin(playerAngle - 2.5) * 7.0));
    glEnd();

    glColor3f(1.0f, 0.86f, 0.18f);
    glBegin(GL_LINE_LOOP);
    for (int segment = 0; segment < segmentCount; ++segment) {
        const double angle = 2.0 * pi * segment / segmentCount;
        glVertex2i(centerX + static_cast<int>(cos(angle) * 12.0),
                   centerY + static_cast<int>(sin(angle) * 12.0));
    }
    glEnd();

    glColor3f(0.70f, 0.82f, 0.92f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int segment = 0; segment < segmentCount; ++segment) {
        const double angle = 2.0 * pi * segment / segmentCount;
        glVertex2i(centerX + static_cast<int>(cos(angle) * radarRadius),
                   centerY + static_cast<int>(sin(angle) * radarRadius));
    }
    glEnd();

    glColor3f(1.0f, 1.0f, 0.9f);
    DrawHudText("2D TOP VIEW", centerX - 38, centerY - radarRadius - 12);
    DrawHudText("N", centerX - 3, centerY - radarRadius - 12);
    DrawHudText("W", centerX - radarRadius - 14, centerY + 5);
    DrawHudText("E", centerX + radarRadius + 8, centerY + 5);
    DrawHudText("S", centerX - 3, centerY + radarRadius + 18);
    DrawHudText("FACING: " + std::string(FacingDirection()), centerX - 38, centerY + radarRadius + 36);
    DrawHudText("P PLAYER  E ENEMY  W WALL", centerX - 76, centerY + radarRadius + 54);
}

void RenderHud() // Draw the heads-up display (HUD) with health, FPS, medkits, weapon status, and controls.
{
    // The HUD uses an orthographic pass so screen-space text does not need 3D transforms.
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
    DrawHudText(std::string(WeaponName()) + " [" + WeaponSlot() + "]" + (fireTimer == 0.0 ? ": READY" : ": FIRING"), width - 250, 30);
    DrawHudText(crouchAmount > 0.5 ? "CROUCHING" : "STANDING", 18, 126);
    RenderMinimap(); // render mini-map after the text so it appears on top of the FPS and medkit counts

    glColor3f(1.0f, 0.92f, 0.62f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2i(width / 2 - 12, height / 2); glVertex2i(width / 2 - 4, height / 2);
    glVertex2i(width / 2 + 4, height / 2); glVertex2i(width / 2 + 12, height / 2);
    glVertex2i(width / 2, height / 2 - 12); glVertex2i(width / 2, height / 2 - 4);
    glVertex2i(width / 2, height / 2 + 4); glVertex2i(width / 2, height / 2 + 12);
    glEnd();
    DrawHudText("ESC - Quit game", 18, height - 24);
    DrawHudText("WASD Move  // SHIFT Run  // CTRL Crouch  // LMB Fire  // SPACE Jump  // P Pause", 18, height - 48);

    if (paused) {
        glColor3f(0.01f, 0.015f, 0.025f);
        glBegin(GL_QUADS);
        glVertex2i(0, 0); glVertex2i(width, 0); glVertex2i(width, height); glVertex2i(0, height);
        glEnd();
        glColor3f(0.12f, 0.16f, 0.22f);
        glBegin(GL_QUADS);
        glVertex2i(width / 2 - 210, height / 2 - 100); glVertex2i(width / 2 + 210, height / 2 - 100);
        glVertex2i(width / 2 + 210, height / 2 + 100); glVertex2i(width / 2 - 210, height / 2 + 100);
        glEnd();
        glColor3f(1.0f, 0.86f, 0.32f);
        DrawHudText("PAUSED", width / 2 - 48, height / 2 - 42);
        glColor3f(1.0f, 1.0f, 0.9f);
        DrawHudText("P - Resume", width / 2 - 62, height / 2 + 4);
        DrawHudText("Q - Quit Game", width / 2 - 78, height / 2 + 38);
    }
    glPopMatrix(); // restore modelview
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

void Update(double dt) {
    // Update is simulation-only; pausing leaves movement, AI, and projectiles frozen.
    if (paused) return;
    const bool crouching = keys[VK_CONTROL];
    const double speed = (crouching ? 2.2 : (keys[VK_SHIFT] ? 7.0 : 4.0)) * dt;
    crouchAmount += ((crouching ? 1.0 : 0.0) - crouchAmount) * std::min(1.0, dt * 12.0);
    breathingTime += dt * 3;
    if (keys[VK_LEFT]) playerAngle -= 2.8 * dt;
    if (keys[VK_RIGHT]) playerAngle += 2.8 * dt;
    double moveX = 0.0, moveZ = 0.0;
    if (keys['W']) { moveX += cos(playerAngle)*speed; moveZ += sin(playerAngle)*speed; }
    if (keys['S']) { moveX -= cos(playerAngle)*speed; moveZ -= sin(playerAngle)*speed; }
    if (keys['A']) { moveX += sin(playerAngle)*speed; moveZ -= cos(playerAngle)*speed; }
    if (keys['D']) { moveX -= sin(playerAngle)*speed; moveZ += cos(playerAngle)*speed; }
    const bool moving = abs(moveX) > 0.001 || abs(moveZ) > 0.001;
    const bool running = moving && keys[VK_SHIFT] && !crouching;
    weaponBobTime += dt * (running ? 13.0 : 9.0);
    weaponBobAmount += ((moving ? 1.0 : 0.0) - weaponBobAmount) * std::min(1.0, dt * 10.0);
    weaponRecoil = std::max(0.0, weaponRecoil - dt * 3.5);
    cameraRecoil = std::max(0.0, cameraRecoil - dt * 2.8);
    if (!PlayerPositionBlocked(playerX + moveX, playerZ)) playerX += moveX;
    if (!PlayerPositionBlocked(playerX, playerZ + moveZ)) playerZ += moveZ;
    if (keys[VK_SPACE] && !crouching && jump == 0.0) jumpSpeed = 5.5;
    jumpSpeed -= 14.0 * dt;
    jump = std::max(0.0, jump + jumpSpeed * dt);
    if (jump == 0.0) jumpSpeed = 0.0;
    for (Cube& enemy : enemies) {
        if (!enemy.alive) continue;
        const double dx = playerX - enemy.x, dz = playerZ - enemy.z;
        const double distance = hypot(dx, dz);
        if (distance > 1.5) {
            const double enemySpeed = 0.65 * dt;
            const double nextX = enemy.x + dx / distance * enemySpeed;
            const double nextZ = enemy.z + dz / distance * enemySpeed;
            if (!EnemyPositionBlocked(nextX, enemy.z)) enemy.x = nextX;
            if (!EnemyPositionBlocked(enemy.x, nextZ)) enemy.z = nextZ;
        }
    }
    enemyFireTimer = std::max(0.0, enemyFireTimer - dt);
    if (enemyFireTimer == 0.0) {
        for (const Cube& enemy : enemies) {
            if (!enemy.alive || hypot(playerX - enemy.x, playerZ - enemy.z) > 14.0 ||
                !ClearLineOfSight(enemy.x, enemy.z, playerX, playerZ)) continue;
            for (EnemyBullet& bullet : enemyBullets) {
                if (bullet.active) continue;
                const double targetY = 1.65 + jump;
                const double targetX = playerX - enemy.x;
                const double targetZ = playerZ - enemy.z;
                const double targetDistance = hypot(hypot(targetX, targetZ), targetY - 0.9);
                bullet = {enemy.x, 0.9, enemy.z, targetX / targetDistance,
                          (targetY - 0.9) / targetDistance, targetZ / targetDistance, true};
                break;
            }
            break;
        }
        enemyFireTimer = 1.2;
    }
    for (EnemyBullet& bullet : enemyBullets) {
        if (!bullet.active) continue;
        bullet.x += bullet.dx * 9.0 * dt;
        bullet.y += bullet.dy * 9.0 * dt;
        bullet.z += bullet.dz * 9.0 * dt;
        if (IsWall(bullet.x, bullet.z)) {
            bullet.active = false;
            continue;
        }
        const double playerDistance = hypot(hypot(bullet.x - playerX, bullet.z - playerZ), bullet.y - (1.65 + jump));
        if (playerDistance < 0.45) {
            playerHealth = std::max(0, playerHealth - 10);
            bullet.active = false;
        }
    }
    for (PlayerTrace& trace : playerTraces) {
        if (!trace.active) continue;
        trace.x += trace.dx * 34.0 * dt;
        trace.y += trace.dy * 34.0 * dt;
        trace.z += trace.dz * 34.0 * dt;
        trace.lifetime -= dt;
        if (trace.lifetime <= 0.0 || IsWall(trace.x, trace.z)) trace.active = false;
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
        const bool shotgun = currentWeapon == Weapon::Shotgun;
        fireTimer = shotgun ? 0.65 : 0.2;
        fireFlash = shotgun ? 0.14 : 0.08;
        weaponRecoil = shotgun ? 0.14 : 0.08;
        cameraRecoil = shotgun ? 0.055 : 0.028;
        PlayWeaponSound();
        SpawnPlayerTrace();
        // Limit shots to the first wall so enemies cannot be hit through cover.
        const double wallDistance = WallDistanceAlongAim();
        for (Cube& enemy : enemies) {
            const double dx = enemy.x-playerX, dz = enemy.z-playerZ, distance = hypot(dx,dz);
            const double difference = atan2(sin(atan2(dz,dx)-playerAngle), cos(atan2(dz,dx)-playerAngle));
            const bool hitByShotgun = shotgun && (abs(difference) < 0.05 || abs(difference - 0.08) < 0.05 || abs(difference + 0.08) < 0.05);
            const bool hitByHandgun = !shotgun && abs(difference) < 0.1;
            if (enemy.alive && distance < wallDistance && distance < 12.0 && (hitByHandgun || hitByShotgun)) { enemy.alive = false; break; }
        }
    }
}

void Render() {
    // Render builds the 3D world first, then draws the first-person weapon and HUD on top.
    glClearColor(0.06f, 0.10f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    const double visualPitch = playerPitch + cameraRecoil;
    const double horizontalLook = cos(visualPitch);
    const double breathingOffset = sin(breathingTime) * 0.012;
    const double eyeHeight = 1.65 - crouchAmount * 0.55;
    gluLookAt(playerX, eyeHeight+jump + breathingOffset, playerZ,
              playerX + cos(playerAngle) * horizontalLook, eyeHeight+jump + breathingOffset + sin(visualPitch),
              playerZ + sin(playerAngle) * horizontalLook, 0.0, 1.0, 0.0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, groundTexture);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS); glVertex3d(-20,0,-20); glVertex3d(40,0,-20); glVertex3d(40,0,40); glVertex3d(-20,0,40); glEnd();
    glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, skyTexture);
    glBegin(GL_QUADS); glVertex3d(-20,3,-20); glVertex3d(-20,3,40); glVertex3d(40,3,40); glVertex3d(40,3,-20); glEnd();
    glEnable(GL_CULL_FACE);
    // Immediate mode is easy to follow but submits every wall repeatedly; a VBO or instanced mesh
    // is the next performance step when the map or object count grows.
    glBindTexture(GL_TEXTURE_2D, brickTexture);
    for (int z=0; z<static_cast<int>(map.size()); ++z) for (int x=0; x<32; ++x)
        if (map[z][x]=='#') WallMesh(x+0.5, z+0.5);
    glDisable(GL_TEXTURE_2D);
    // Keep static state changes outside the object loops so each object only submits its geometry.
    for (const Cube& enemy : enemies) if (enemy.alive) CubeMesh(enemy.x, 0.75, enemy.z, 0.8, 0.78f, 0.12f, 0.10f);
    for (const Medkit& medkit : medkits) if (medkit.available) {
        CubeMesh(medkit.x, 0.35, medkit.z, 0.55, 0.10f, 0.78f, 0.25f);
        CubeMesh(medkit.x, 0.35, medkit.z - 0.29, 0.22, 0.95f, 0.95f, 0.90f);
    }
    for (const EnemyBullet& bullet : enemyBullets)
        if (bullet.active) CubeMesh(bullet.x, bullet.y, bullet.z, 0.12, 1.0f, 0.25f, 0.08f);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(currentWeapon == Weapon::Shotgun ? 3.0f : 2.0f);
    glColor3f(1.0f, currentWeapon == Weapon::Shotgun ? 0.35f : 0.85f, 0.08f);
    glBegin(GL_LINES);
    for (const PlayerTrace& trace : playerTraces) {
        if (!trace.active) continue;
        glVertex3d(trace.x, trace.y, trace.z);
        glVertex3d(trace.x - trace.dx * 0.55, trace.y - trace.dy * 0.55, trace.z - trace.dz * 0.55);
    }
    glEnd();
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glLoadIdentity();
    const double bobX = sin(weaponBobTime) * 0.025 * weaponBobAmount;
    const double bobY = abs(cos(weaponBobTime)) * 0.018 * weaponBobAmount;
    glTranslated(bobX, bobY - weaponRecoil + breathingOffset * 0.55 - crouchAmount * 0.16, weaponRecoil * 0.35);
    glRotated(sin(weaponBobTime) * 1.5 * weaponBobAmount, 0.0, 0.0, 1.0);
    if (currentWeapon == Weapon::Handgun) RenderHandgun();
    else RenderShotgun();
    if (fireFlash > 0.0) {
        glDisable(GL_DEPTH_TEST);
        glColor3f(1.0f, currentWeapon == Weapon::Shotgun ? 0.38f : 0.72f, 0.08f);
        glBegin(GL_TRIANGLES);
        // Place the flash beyond the barrel exits so the VFX never cuts through the weapon model.
        const auto drawMuzzleFlash = [](double x, double y, double muzzleZ, double flashSize) {
            glVertex3d(x, y, muzzleZ - flashSize * 1.8);
            glVertex3d(x - flashSize, y, muzzleZ);
            glVertex3d(x, y + flashSize, muzzleZ);
            glVertex3d(x, y, muzzleZ - flashSize * 1.8);
            glVertex3d(x + flashSize, y, muzzleZ);
            glVertex3d(x, y - flashSize, muzzleZ);
        };
        if (currentWeapon == Weapon::Shotgun) {
            drawMuzzleFlash(0.47, -0.30, -1.38, 0.12);
            drawMuzzleFlash(0.57, -0.30, -1.38, 0.12);
        } else {
            drawMuzzleFlash(0.52, -0.25, -1.38, 0.10);
        }
        glEnd();
        glEnable(GL_DEPTH_TEST);
    }
    glPopMatrix();
    glEnable(GL_CULL_FACE);
    // Draw all player-facing UI over the completed 3D frame before presenting it.
    RenderHud();
    SwapBuffers(deviceContext);
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    // Windows messages provide keyboard, mouse, focus, resize, and shutdown events.
    switch (message) {
    case WM_KEYDOWN:
        if (wParam<256) keys[wParam]=true;
        if (wParam=='1') currentWeapon = Weapon::Handgun;
        if (wParam=='2') currentWeapon = Weapon::Shotgun;
        if (wParam=='P' && !(lParam & 0x40000000)) SetGamePaused(window, !paused);
        if (wParam=='Q' && paused) { StopRockMusic(); PostQuitMessage(0); }
        if (wParam==VK_ESCAPE) { StopRockMusic(); PostQuitMessage(0); }
        return 0;
    case WM_KEYUP: if (wParam<256) keys[wParam]=false; return 0;
    case WM_MOUSEMOVE:
    {
        if (!mouseLookCaptured) return 0;
        if (ignoreMouseMove) {
            ignoreMouseMove = false;
            return 0;
        }
        RECT clientRect = {};
        GetClientRect(window, &clientRect);
        const int centerX = (clientRect.right - clientRect.left) / 2;
        const int centerY = (clientRect.bottom - clientRect.top) / 2;
        const int mouseX = static_cast<int>(static_cast<short>(LOWORD(lParam)));
        const int mouseY = static_cast<int>(static_cast<short>(HIWORD(lParam)));
        playerAngle += (mouseX - centerX) * 0.004;
        playerPitch = std::clamp(playerPitch - (mouseY - centerY) * 0.003, -1.2, 1.2);
        CenterMouse(window);
        return 0;
    }
    case WM_SETFOCUS: SetMouseLookCaptured(window, true); return 0;
    case WM_KILLFOCUS: SetMouseLookCaptured(window, false); return 0;
    case WM_SETCURSOR:
        if (mouseLookCaptured) { SetCursor(nullptr); return TRUE; }
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_LBUTTONDOWN: keys[VK_LBUTTON]=true; return 0;
    case WM_LBUTTONUP: keys[VK_LBUTTON]=false; return 0;
    case WM_SIZE:
        glViewport(0,0,LOWORD(lParam),HIWORD(lParam)); glMatrixMode(GL_PROJECTION); glLoadIdentity();
        gluPerspective(70.0, HIWORD(lParam) ? static_cast<double>(LOWORD(lParam))/HIWORD(lParam) : 1.0, 0.05, 100.0);
        glMatrixMode(GL_MODELVIEW); return 0;
    case WM_DESTROY: SetMouseLookCaptured(window, false); StopRockMusic(); PostQuitMessage(0); return 0;
    default: return DefWindowProcW(window,message,wParam,lParam);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    // Application startup creates the Win32 window, OpenGL context, audio, and game loop.
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
    CreateWeaponSounds();
    CreateRockMusic();
    StartRockMusic();
    CreateBrickTexture();
    CreateEnvironmentTextures();
    CreateHudFont();
    gluPerspective(70.0,static_cast<double>(width)/height,0.05,100.0); ShowWindow(windowHandle,showCommand);
    fpsClock=std::chrono::steady_clock::now(); auto previous=fpsClock; MSG message={};
    while (message.message!=WM_QUIT) {
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        // Clamp long pauses so a breakpoint or window drag cannot teleport the player through walls.
        const auto now=std::chrono::steady_clock::now(); const double dt=std::min(0.1,std::chrono::duration<double>(now-previous).count()); previous=now;
        Update(dt); Render(); ++frameCount;
        if (std::chrono::duration<double>(now-fpsClock).count()>=1.0) {
            displayedFps=frameCount; frameCount=0; fpsClock=now;
        }
    }
    StopRockMusic();
    wglMakeCurrent(nullptr,nullptr); wglDeleteContext(renderContext); ReleaseDC(windowHandle,deviceContext); return 0;
}
