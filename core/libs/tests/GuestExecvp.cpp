#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>
extern "C" int APS5_VABI execvp_nid_postfix(const char*, char* const*);
static void Require(bool value) { if (!value) std::abort(); }
static bool ReportsNotImplemented(const char* file, char* const* arguments) {
    try {
        execvp_nid_postfix(file, arguments);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == "execvp: executable replacement not implemented";
    }
    return false;
}
int main() {
    char shell[] = "sh";
    char option[] = "-c";
    char command[] = "exit 0";
    char* arguments[] = {shell, option, command, nullptr};
    Require(ReportsNotImplemented("sh", arguments));
    Require(ReportsNotImplemented("/bin/sh", arguments));
    Require(ReportsNotImplemented(nullptr, nullptr));
}
