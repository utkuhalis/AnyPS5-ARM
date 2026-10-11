#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
extern "C" int APS5_VABI execvp_nid_postfix(const char*, char* const*);
extern "C" int* APS5_VABI __error_nid_postfix();
static void Require(bool value) { if (!value) std::abort(); }
static bool Fails(const char* file, char* const* arguments, int error) {
    *__error_nid_postfix() = 0;
    return execvp_nid_postfix(file, arguments) == -1 && *__error_nid_postfix() == error;
}
int main() {
    char shell[] = "sh";
    char option[] = "-c";
    char command[] = "exit 0";
    char* arguments[] = {shell, option, command, nullptr};
    Require(Fails("sh", arguments, 78));
    Require(Fails("/bin/sh", arguments, 78));
    Require(Fails("", arguments, 2));
    Require(Fails(nullptr, nullptr, 14));
}
