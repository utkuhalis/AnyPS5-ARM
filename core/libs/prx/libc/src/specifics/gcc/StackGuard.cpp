#include <cstdint>
#include <cstdlib>

// Darwin's own stack protector reads a global __stack_chk_guard from libSystem, and two-level binding
// would tie every library's host code to this one if it had the same name. The postfixed name gets
// the same NID.
#ifdef __APPLE__
extern "C" std::uintptr_t __stack_chk_guard_nid_postfix = 0xDEADBEEFCAFEBABEull;
#else
std::uintptr_t __stack_chk_guard = 0xDEADBEEFCAFEBABEull;
#endif
