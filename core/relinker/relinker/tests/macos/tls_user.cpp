// A guest module that reads another module's TLS variable: general-dynamic access gives DTPMOD64 and
// DTPOFF64 relocations against an imported symbol.
extern __thread int shared;

extern "C" int readShared() { return shared; }
