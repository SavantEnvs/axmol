// Fuzz ax::SAXParser::parse(data, len, option) (axmol/platform/SAXParser.cpp over 3rdparty/xsxml): the XML
// reader behind plist (FileUtils::getValueMapFromData), TMX maps and RichText. First byte picks the
// parse option (NORMAL / HTML / TRIM_WHITESPACE); the delegate walks every callback argument so
// out-of-bounds names, attributes or text lengths are touched.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "axmol/platform/SAXParser.h"

namespace
{
struct Delegate : ax::SAXDelegator
{
    size_t sum = 0;
    void startElement(void*, const char* name, const char** atts) override
    {
        sum += strlen(name);
        for (int i = 0; atts && atts[i]; ++i)
            sum += strlen(atts[i]);
    }
    void endElement(void*, const char* name) override { sum += strlen(name); }
    void textHandler(void*, const char* s, size_t len) override
    {
        for (size_t i = 0; i < len; ++i)
            sum += static_cast<unsigned char>(s[i]);
    }
};
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1)
        return -1;
    using Opt = ax::SAXParser::ParseOption;
    static const Opt kOpts[] = {Opt::NORMAL, Opt::HTML, Opt::TRIM_WHITESPACE};
    const Opt opt = kOpts[data[0] % 3];
    Delegate d;
    ax::SAXParser parser;
    parser.setDelegator(&d);
    parser.parse(reinterpret_cast<const char*>(data + 1), size - 1, opt);
    return 0;
}
