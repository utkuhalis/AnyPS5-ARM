// A small game in the shape of a PS5 title: Breakout drawn by the CPU into two linear display buffers
// from direct memory and flipped through video out, played with the pad (or the keyboard mapped onto
// it), with square-wave sound on its own thread through audio out.
//
//     eboot [seconds]
//
// Without input for three seconds the paddle follows the ball by itself. With a number of seconds the
// game exits after that long with the count of bricks broken; Options (Escape) exits at any time.
struct BufferAttribute {
    unsigned reserved0, tilingMode, aspectRatio, width, height, pitchInPixel;
    unsigned long long option, pixelFormat, dccClearColor;
    unsigned dccControl, pad0;
    unsigned long long reserved1[3];
};
struct Buffers {
    const void* data;
    const void* metadata;
    const void* reserved[2];
};
struct PadData {
    unsigned buttons;
    unsigned char leftStickX, leftStickY, rightStickX, rightStickY, analogL2, analogR2, padding[2];
    unsigned char rest[0x80];   // the rest of the 0x78-byte ScePadData, with room to spare
};

extern "C" {
int sceVideoOutOpen(int userId, int busType, int index, const void* parameter);
void sceVideoOutSetBufferAttribute2(BufferAttribute* attribute, unsigned long long pixelFormat, unsigned tilingMode, unsigned width, unsigned height, unsigned long long option, unsigned dccControl, unsigned long long dccClearColor);
int sceVideoOutRegisterBuffers2(int handle, int setIndex, int bufferIndexStart, const Buffers* buffers, int bufferCount, const BufferAttribute* attribute, int category, const void* option);
int sceVideoOutSubmitFlip(int handle, int index, int flipMode, long long flipArgument);
int sceVideoOutWaitVblank(int handle);
int sceVideoOutClose(int handle);
int sceKernelAllocateDirectMemory(long long searchStart, long long searchEnd, unsigned long length, unsigned long alignment, int memoryType, long long* physicalAddress);
int sceKernelMapDirectMemory(void** address, unsigned long length, int protection, int flags, long long physicalAddress, unsigned long alignment);
int scePthreadCreate(void** thread, const void* attr, void* (*entry)(void*), void* arg, const char* name);
int sceUserServiceInitialize(const void* parameters);
int sceUserServiceGetInitialUser(int* user);
int scePadInit();
int scePadOpen(int user, int type, int index, const void* parameter);
int scePadReadState(int handle, PadData* data);
int sceAudioOutInit();
int sceAudioOutOpen(int user, int type, int index, unsigned length, unsigned frequency, unsigned format);
int sceAudioOutOutput(int handle, const void* samples);
[[noreturn]] void exit(int);
}

// Called through a data pointer, so the executable also has a RELA import.
int (*volatile submitFlip)(int, int, int, long long) = sceVideoOutSubmitFlip;

namespace {

constexpr int Width = 640;
constexpr int Height = 360;
constexpr unsigned long BufferBytes = (Width * Height * 4 + 65535) / 65536 * 65536;
constexpr int FlipQueueFull = static_cast<int>(0x80290012u);
constexpr unsigned ButtonOptions = 0x0008, ButtonRight = 0x0020, ButtonLeft = 0x0080, ButtonCross = 0x4000;

constexpr int Columns = 10, Rows = 5;
constexpr int BrickWidth = 56, BrickHeight = 14, BrickGap = 6, BricksTop = 48;
constexpr int BricksLeft = (Width - (Columns * BrickWidth + (Columns - 1) * BrickGap)) / 2;
constexpr int PaddleWidth = 84, PaddleHeight = 8, PaddleY = Height - 28;
constexpr int BallSize = 6;
constexpr unsigned RowColors[Rows] = {0xffe5484d, 0xfff76b15, 0xffffc53d, 0xff46a758, 0xff3e63dd};

unsigned* screen;

void Fill(int x, int y, int w, int h, unsigned color) {
    for (int row = y < 0 ? 0 : y; row < y + h && row < Height; ++row)
        for (int column = x < 0 ? 0 : x; column < x + w && column < Width; ++column) screen[row * Width + column] = color;
}

// 3x5 digits, one bit per cell, rows top to bottom.
constexpr unsigned short Digits[10] = {075557, 022222, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717};

void Number(int x, int y, int value, int scale, unsigned color) {
    char text[8];
    int count = 0;
    do { text[count++] = static_cast<char>(value % 10); value /= 10; } while (value != 0 && count < 8);
    for (int index = count - 1; index >= 0; --index, x += 4 * scale) {
        const unsigned glyph = Digits[static_cast<int>(text[index])];
        for (int row = 0; row < 5; ++row)
            for (int column = 0; column < 3; ++column)
                if (glyph & (1u << ((4 - row) * 3 + (2 - column)))) Fill(x + column * scale, y + row * scale, scale, scale, color);
    }
}

// The sound thread plays one square-wave tone at a time, 48 kHz mono, 256 samples per block.
struct Tone {
    volatile int frequency;
    volatile int samples;
} tone;

void Beep(int frequency, int milliseconds) {
    tone.samples = 0;
    tone.frequency = frequency;
    tone.samples = 48 * milliseconds;
}

void* Sound(void* argument) {
    const int port = static_cast<int>(reinterpret_cast<long>(argument));
    short block[256];
    unsigned phase = 0;
    for (;;) {
        const int frequency = tone.frequency;
        for (short& sample : block) {
            const int left = tone.samples;
            if (left <= 0 || frequency <= 0) { sample = 0; continue; }
            tone.samples = left - 1;
            phase += static_cast<unsigned>(frequency) * 89478u;   // 2^32 / 48000
            const int envelope = left > 2400 ? 2400 : left;          // fade out over the last 50 ms
            sample = static_cast<short>((phase & 0x80000000u ? 2600 : -2600) * envelope / 2400);
        }
        sceAudioOutOutput(port, block);
    }
}

struct Game {
    bool bricks[Rows][Columns];
    int left, broken, score, lives;
    float paddle, ballX, ballY, velocityX, velocityY;
    bool served;

    void ResetBricks() {
        for (auto& row : bricks)
            for (bool& brick : row) brick = true;
        left = Rows * Columns;
    }

    void Serve() {
        ballX = paddle + PaddleWidth / 2 - BallSize / 2;
        ballY = PaddleY - BallSize - 1;
        velocityX = 2.6f;
        velocityY = -3.4f;
        served = false;
    }

    void Start() {
        ResetBricks();
        score = 0;
        lives = 3;
        paddle = (Width - PaddleWidth) / 2;
        Serve();
    }

    // One 60 Hz step; direction is -1, 0 or 1 or the stick in [-1, 1].
    void Step(float direction, bool launch) {
        paddle += direction * 7.0f;
        if (paddle < 0) paddle = 0;
        if (paddle > Width - PaddleWidth) paddle = Width - PaddleWidth;
        if (!served) {
            ballX = paddle + PaddleWidth / 2 - BallSize / 2;
            if (launch) served = true;
            return;
        }
        ballX += velocityX;
        ballY += velocityY;
        if (ballX < 0) { ballX = 0; velocityX = -velocityX; Beep(660, 30); }
        if (ballX > Width - BallSize) { ballX = Width - BallSize; velocityX = -velocityX; Beep(660, 30); }
        if (ballY < 24) { ballY = 24; velocityY = -velocityY; Beep(660, 30); }
        // The paddle: the bounce angle follows where the ball hits it.
        if (velocityY > 0 && ballY + BallSize >= PaddleY && ballY + BallSize <= PaddleY + PaddleHeight + 6 &&
            ballX + BallSize >= paddle && ballX <= paddle + PaddleWidth) {
            const float offset = (ballX + BallSize / 2 - (paddle + PaddleWidth / 2)) / (PaddleWidth / 2);
            const float speed = 4.2f + static_cast<float>(broken % 50) * 0.04f;
            velocityX = offset * speed * 0.9f;
            velocityY = -(speed - (offset < 0 ? -offset : offset) * 1.2f);
            ballY = PaddleY - BallSize;
            Beep(440, 40);
        }
        // Bricks: the first one the ball overlaps; reflect on the shallower axis.
        for (int row = 0; row < Rows; ++row) {
            for (int column = 0; column < Columns; ++column) {
                if (!bricks[row][column]) continue;
                const float x = BricksLeft + column * (BrickWidth + BrickGap);
                const float y = BricksTop + row * (BrickHeight + BrickGap);
                if (ballX + BallSize <= x || ballX >= x + BrickWidth || ballY + BallSize <= y || ballY >= y + BrickHeight) continue;
                bricks[row][column] = false;
                --left;
                ++broken;
                score += (Rows - row) * 10;
                const float overlapX = velocityX > 0 ? ballX + BallSize - x : x + BrickWidth - ballX;
                const float overlapY = velocityY > 0 ? ballY + BallSize - y : y + BrickHeight - ballY;
                if (overlapX < overlapY) velocityX = -velocityX; else velocityY = -velocityY;
                Beep(880 + (Rows - row) * 110, 50);
                if (left == 0) { ResetBricks(); Serve(); Beep(1320, 300); }
                return;
            }
        }
        if (ballY > Height) {
            Beep(196, 250);
            if (--lives == 0) Start(); else Serve();
        }
    }

    void Draw(bool demo, int seconds) const {
        for (int y = 0; y < Height; ++y) {
            const unsigned shade = 0x10 + static_cast<unsigned>(y) * 0x10 / Height;
            const unsigned background = 0xff000000u | (shade << 16) | (shade << 8) | (shade + 0x18);
            for (int x = 0; x < Width; ++x) screen[y * Width + x] = background;
        }
        Fill(0, 22, Width, 1, 0xff3a4050);
        for (int row = 0; row < Rows; ++row)
            for (int column = 0; column < Columns; ++column)
                if (bricks[row][column]) {
                    const int x = BricksLeft + column * (BrickWidth + BrickGap), y = BricksTop + row * (BrickHeight + BrickGap);
                    Fill(x, y, BrickWidth, BrickHeight, RowColors[row]);
                    Fill(x, y, BrickWidth, 2, 0xffffffff & (RowColors[row] | 0xff404040));
                }
        Fill(static_cast<int>(paddle), PaddleY, PaddleWidth, PaddleHeight, 0xffe8ecf2);
        Fill(static_cast<int>(ballX), static_cast<int>(ballY), BallSize, BallSize, 0xffffffff);
        Number(8, 4, score, 3, 0xffe8ecf2);
        for (int life = 0; life < lives; ++life) Fill(Width - 16 - life * 14, 8, 10, 8, 0xffe5484d);
        if (demo) Fill(Width / 2 - 20, 9, 40, 6, 0xff46a758);        // the green bar: the paddle plays itself
        if (seconds >= 0) Number(Width / 2 + 34, 4, seconds, 3, 0xff8b95a7);
    }
};

Game game;

int ParseSeconds(const char* text) {
    int value = 0;
    for (; *text >= '0' && *text <= '9'; ++text) value = value * 10 + (*text - '0');
    return value;
}

}

extern "C" [[noreturn]] void _start(void* block) {
    const auto* words = static_cast<const unsigned long long*>(block);
    const auto* arguments = reinterpret_cast<char* const*>(words + 1);
    const int limit = words[0] > 1 ? ParseSeconds(arguments[1]) : 0;

    sceUserServiceInitialize(nullptr);
    int user = 0;
    if (sceUserServiceGetInitialUser(&user) != 0) exit(101);
    const int video = sceVideoOutOpen(255, 0, 0, nullptr);
    if (video <= 0) exit(102);
    void* pixels[2];
    for (auto& buffer : pixels) {
        long long physical = 0;
        void* mapped = nullptr;
        if (sceKernelAllocateDirectMemory(0, 0x7fffffffffll, BufferBytes, 65536, 0, &physical) != 0) exit(103);
        if (sceKernelMapDirectMemory(&mapped, BufferBytes, 0x33, 0, physical, 65536) != 0) exit(104);
        buffer = mapped;
    }
    BufferAttribute attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 1, Width, Height, 0, 0, 0);
    const Buffers buffers[2] = {{pixels[0], nullptr, {}}, {pixels[1], nullptr, {}}};
    if (sceVideoOutRegisterBuffers2(video, 0, 0, buffers, 2, &attribute, 0, nullptr) != 0) exit(105);
    scePadInit();
    const int pad = scePadOpen(user, 0, 0, nullptr);
    sceAudioOutInit();
    const int audio = sceAudioOutOpen(user, 0, 0, 256, 48000, 0);
    void* soundThread = nullptr;
    if (audio > 0) scePthreadCreate(&soundThread, nullptr, Sound, reinterpret_cast<void*>(static_cast<long>(audio)), "sound");

    game.Start();
    int idle = 0;
    unsigned previous = 0;
    for (long long frame = 0;; ++frame) {
        PadData state{};
        const bool padRead = pad > 0 && scePadReadState(pad, &state) == 0;
        const unsigned buttons = padRead ? state.buttons : 0u;
        if (buttons & ButtonOptions) exit(game.broken > 250 ? 250 : game.broken);
        float direction = (buttons & ButtonLeft ? -1.0f : 0.0f) + (buttons & ButtonRight ? 1.0f : 0.0f);
        const int stick = padRead ? state.leftStickX - 128 : 0;
        if (stick < -24 || stick > 24) direction = static_cast<float>(stick) / 127.0f;
        const bool launch = (buttons & ButtonCross) && !(previous & ButtonCross);
        previous = buttons;
        idle = direction != 0.0f || launch ? 0 : idle + 1;
        const bool demo = idle > 180;
        if (demo) {
            const float target = game.ballX + BallSize / 2 - (game.paddle + PaddleWidth / 2);
            direction = target > 6 ? 1.0f : target < -6 ? -1.0f : target / 6;
        }
        game.Step(direction, launch || demo);
        const int elapsed = static_cast<int>(frame / 60);
        if (limit > 0 && elapsed >= limit) exit(game.broken > 250 ? 250 : game.broken);

        screen = static_cast<unsigned*>(pixels[frame & 1]);
        game.Draw(demo, limit > 0 ? limit - elapsed : -1);
        int result;
        for (int retries = 0; (result = submitFlip(video, static_cast<int>(frame & 1), 1, frame)) == FlipQueueFull; ++retries) {
            if (retries == 600) exit(106);
            sceVideoOutWaitVblank(video);
        }
        if (result != 0) exit(107);
        sceVideoOutWaitVblank(video);
    }
}
