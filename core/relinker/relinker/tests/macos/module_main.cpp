extern "C" int greet(int);
extern "C" [[noreturn]] void exit(int);

struct ModuleInfoForUnwind {
    unsigned long long size;
    char name[256];
    unsigned long long ehFrameHeader;
    unsigned long long ehFrame;
    unsigned long long ehFrameSize;
    unsigned long long segment;
    unsigned long long segmentSize;
};
extern "C" int sceKernelGetModuleInfoForUnwind(unsigned long long address, int flags, ModuleInfoForUnwind* info);

// Called through a data pointer, so the executable also has a RELA import (R_X86_64_64/GLOB_DAT).
int (*volatile greetPointer)(int) = greet;

extern "C" [[noreturn]] void _start(void*) {
    ModuleInfoForUnwind info {};
    const auto address = reinterpret_cast<unsigned long long>(greetPointer);
    bool named = false;
    for (int index = 0; info.name[index] != 0 && index + 5 < 256; ++index)
        if (info.name[index] == 'g' && info.name[index + 1] == 'r' && info.name[index + 2] == 'e' && info.name[index + 3] == 'e' && info.name[index + 4] == 't') named = true;
    const bool found = sceKernelGetModuleInfoForUnwind(address, 0, &info) == 0 && info.ehFrameHeader != 0 && info.segmentSize != 0;
    for (int index = 0; found && info.name[index] != 0 && index + 5 < 256; ++index)
        if (info.name[index] == 'g' && info.name[index + 1] == 'r' && info.name[index + 2] == 'e' && info.name[index + 3] == 'e' && info.name[index + 4] == 't') named = true;
    exit(greetPointer(21) + (found && named ? 100 : 0));
}
