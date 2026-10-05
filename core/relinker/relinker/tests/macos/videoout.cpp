// A guest that draws: it opens video out, maps two linear display buffers from direct memory and
// flips a moving pattern for a few seconds.
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

extern "C" {
int sceVideoOutOpen(int userId, int busType, int index, const void* parameter);
void sceVideoOutSetBufferAttribute2(BufferAttribute* attribute, unsigned long long pixelFormat, unsigned tilingMode, unsigned width, unsigned height, unsigned long long option, unsigned dccControl, unsigned long long dccClearColor);
int sceVideoOutRegisterBuffers2(int handle, int setIndex, int bufferIndexStart, const Buffers* buffers, int bufferCount, const BufferAttribute* attribute, int category, const void* option);
int sceVideoOutSubmitFlip(int handle, int index, int flipMode, long long flipArgument);
int sceVideoOutWaitVblank(int handle);
int sceVideoOutClose(int handle);
int sceKernelAllocateDirectMemory(long long searchStart, long long searchEnd, unsigned long length, unsigned long alignment, int memoryType, long long* physicalAddress);
int sceKernelMapDirectMemory(void** address, unsigned long length, int protection, int flags, long long physicalAddress, unsigned long alignment);
int puts(const char*);
[[noreturn]] void exit(int);
}

// Called through a data pointer, so the executable also has a RELA import.
int (*volatile submitFlip)(int, int, int, long long) = sceVideoOutSubmitFlip;

constexpr unsigned Width = 640;
constexpr unsigned Height = 360;
constexpr unsigned long BufferBytes = (Width * Height * 4 + 65535) / 65536 * 65536;
constexpr int Frames = 300;
// SCE_VIDEO_OUT_ERROR_FLIP_QUEUE_FULL: the presenter is behind (its first frame builds the Vulkan
// pipelines), so wait a vblank and submit again like a title does.
constexpr int FlipQueueFull = static_cast<int>(0x80290012u);
constexpr int FlipRetryVblanks = 600;

extern "C" [[noreturn]] void _start(void*) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    if (handle <= 0) exit(1);
    unsigned* pixels[2];
    Buffers buffers[2] {};
    for (int index = 0; index < 2; ++index) {
        long long physical = 0;
        void* mapped = nullptr;
        if (sceKernelAllocateDirectMemory(0, 0x7fffffffffll, BufferBytes, 65536, 0, &physical) != 0) exit(2);
        if (sceKernelMapDirectMemory(&mapped, BufferBytes, 0x33, 0, physical, 65536) != 0) exit(3);
        pixels[index] = static_cast<unsigned*>(mapped);
        buffers[index].data = mapped;
    }
    BufferAttribute attribute {};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 1, Width, Height, 0, 0, 0);
    if (sceVideoOutRegisterBuffers2(handle, 0, 0, buffers, 2, &attribute, 0, nullptr) != 0) exit(4);
    puts("guest video out: drawing");
    for (int frame = 0; frame < Frames; ++frame) {
        unsigned* target = pixels[frame & 1];
        for (unsigned y = 0; y < Height; ++y) {
            for (unsigned x = 0; x < Width; ++x) {
                const unsigned red = (x + frame * 2) & 255;
                const unsigned green = (y + frame) & 255;
                const unsigned blue = ((x ^ y) + frame * 3) & 255;
                target[y * Width + x] = 0xff000000u | (red << 16) | (green << 8) | blue;
            }
        }
        int result;
        for (int retries = 0; (result = submitFlip(handle, frame & 1, 1, frame)) == FlipQueueFull; ++retries) {
            if (retries == FlipRetryVblanks) exit(6);
            sceVideoOutWaitVblank(handle);
        }
        if (result != 0) exit(5);
        sceVideoOutWaitVblank(handle);
    }
    sceVideoOutClose(handle);
    puts("guest video out: done");
    exit(0);
}
