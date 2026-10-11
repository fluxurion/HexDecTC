// Link shims for the prebuilt (clang + MSVC STL) skia.lib.
//
// 1) __std_min_f / __std_max_f / __std_minmax_f are MSVC STL helpers
//    that live in msvcprt.lib (msvcp140). We link the static CRT (/MT)
//    where they do not exist, so provide them here.
// 2) freetype's gzip font loader (ftgzip, pulled in via the FT module
//    registry) wants vanilla zlib entry points, but the bundled
//    Chromium zlib exports Cr_z_* prefixed names. We never load
//    compressed fonts, so stub them out with Z_STREAM_ERROR.

extern "C" {

float __std_min_f(float a, float b) { return b < a ? b : a; }
float __std_max_f(float a, float b) { return a < b ? b : a; }

struct StdMinMaxF {
    float lo, hi;
};
StdMinMaxF __std_minmax_f(float a, float b) {
    return a < b ? StdMinMaxF{a, b} : StdMinMaxF{b, a};
}

// zlib stubs — dead code path (we never feed Skia gzip'd fonts).
int inflateInit2_(void*, int, const char*, int) { return -2; }
int inflate(void*, int) { return -2; }
int inflateEnd(void*) { return -2; }
int inflateReset(void*) { return -2; }

} // extern "C"
