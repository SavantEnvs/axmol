// Known-answer functional test for the axmol core decoders the TGA and SAX fuzz targets reach
// (axmol/base/TGAlib.cpp, axmol/platform/SAXParser.cpp), built with normal flags.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "axmol/base/TGAlib.h"
#include "axmol/platform/SAXParser.h"

static int g_pass, g_fail;
static void check(const char* name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? g_pass : g_fail)++;
}

static std::vector<unsigned char> tgaHeader(int type, int w, int h, int depth, int desc)
{
    std::vector<unsigned char> b(18, 0);
    b[2]  = (unsigned char)type;
    b[12] = w & 0xff; b[13] = (w >> 8) & 0xff;
    b[14] = h & 0xff; b[15] = (h >> 8) & 0xff;
    b[16] = (unsigned char)depth;
    b[17] = (unsigned char)desc;
    return b;
}

static int tgaStatus(std::vector<unsigned char> b)
{
    ax::tImageTGA* t = ax::tgaLoadBuffer(b.data(), (int32_t)b.size());
    int s = t ? t->status : -100;
    if (t && s == ax::TGA_OK)
        ax::tgaDestroy(t);
    else
        free(t);
    return s;
}

struct Recorder : ax::SAXDelegator
{
    std::string log;
    void startElement(void*, const char* name, const char** atts) override
    {
        log += "<";
        log += name;
        for (int i = 0; atts && atts[i]; i += 2)
            log += std::string(" ") + atts[i] + "=" + (atts[i + 1] ? atts[i + 1] : "");
        log += ">";
    }
    void endElement(void*, const char* name) override { log += std::string("</") + name + ">"; }
    void textHandler(void*, const char* s, size_t len) override { log += "[" + std::string(s, len) + "]"; }
};

static std::string sax(const char* xml, ax::SAXParser::ParseOption opt, bool* ok)
{
    Recorder r;
    ax::SAXParser p;
    p.setDelegator(&r);
    *ok = p.parse(xml, strlen(xml), opt);
    return r.log;
}

int main()
{
    // 2x2 24-bit truecolor, top-left origin (descriptor 0x20 -> flipped): BGR -> RGB swap, then row flip.
    auto b = tgaHeader(2, 2, 2, 24, 0x20);
    const unsigned char px[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    b.insert(b.end(), px, px + sizeof(px));
    ax::tImageTGA* t = ax::tgaLoadBuffer(b.data(), (int32_t)b.size());
    const unsigned char expect[] = {9, 8, 7, 12, 11, 10, 3, 2, 1, 6, 5, 4};
    check("tga_raw_ok", t && t->status == ax::TGA_OK && t->width == 2 && t->height == 2 && t->pixelDepth == 24);
    check("tga_raw_pixels", t && t->status == ax::TGA_OK && memcmp(t->imageData, expect, sizeof(expect)) == 0);
    if (t && t->status == ax::TGA_OK) ax::tgaDestroy(t); else free(t);

    // RLE (type 10), 3x1 24-bit: one run packet of 3 pixels (0x82) with BGR (10,20,30).
    auto r = tgaHeader(10, 3, 1, 24, 0);
    const unsigned char rle[] = {0x82, 10, 20, 30};
    r.insert(r.end(), rle, rle + sizeof(rle));
    t = ax::tgaLoadBuffer(r.data(), (int32_t)r.size());
    const unsigned char rexp[] = {30, 20, 10, 30, 20, 10, 30, 20, 10};
    check("tga_rle_pixels", t && t->status == ax::TGA_OK && memcmp(t->imageData, rexp, sizeof(rexp)) == 0);
    if (t && t->status == ax::TGA_OK) ax::tgaDestroy(t); else free(t);

    check("tga_indexed_rejected", tgaStatus(tgaHeader(1, 2, 2, 8, 0)) == ax::TGA_ERROR_INDEXED_COLOR);
    check("tga_compressed_rejected", tgaStatus(tgaHeader(9, 2, 2, 8, 0)) == ax::TGA_ERROR_COMPRESSED_FILE);
    check("tga_truncated_rejected", tgaStatus(tgaHeader(2, 4, 4, 32, 0)) == ax::TGA_ERROR_READING_FILE);

    bool ok = false;
    auto log = sax("<a x=\"1\" y=\"two\"><b>hi &amp; bye</b><c/></a>", ax::SAXParser::ParseOption::NORMAL, &ok);
    printf("  sax log: %s\n", log.c_str());
    check("sax_normal", ok && log == "<a x=1 y=two><b>[hi & bye]</b><c></c></a>");
    log = sax("<p>  a   b  </p>", ax::SAXParser::ParseOption::TRIM_WHITESPACE, &ok);
    printf("  sax trim log: %s\n", log.c_str());
    check("sax_trim_whitespace", ok && log == "<p>[a   b]</p>");
    log = sax("<a><b></a>", ax::SAXParser::ParseOption::NORMAL, &ok);
    check("sax_mismatched_rejected", !ok);

    printf("RESULT passed=%d failed=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
