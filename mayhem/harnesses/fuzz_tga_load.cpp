// Fuzz axmol's TGA decoder (axmol/base/TGAlib.cpp): tgaLoadBuffer is what ax::Image uses for .tga
// assets — header parsing, raw and RLE image data, vertical flip. (tgaRGBtogreyscale is not called: the
// engine never applies it to loaded images.)
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "axmol/base/TGAlib.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > 0x7fffffff)
        return -1;  // tgaLoadBuffer takes an int32_t size
    std::vector<unsigned char> buf(data, data + size);  // exact-size copy: overreads hit ASan
    ax::tImageTGA* img = ax::tgaLoadBuffer(buf.data(), static_cast<int32_t>(size));
    // Ownership as ax::Image::initWithImageData (axmol/platform/Image.cpp) handles it: imageData is only
    // initialized/owned on TGA_OK; on any other status the engine frees just the struct.
    if (img && img->status == ax::TGA_OK)
    {
        ax::tgaDestroy(img);
    }
    else
    {
        free(img);
    }
    return 0;
}
