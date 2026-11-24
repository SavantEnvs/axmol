// Fuzz ax::network::Uri::parse (axmol/network/Uri.cpp): the URL parser HttpClient / Downloader /
// WebSocket use on URLs that can come from game data or a server. Exercises the regex split,
// authority (user:pass@host:port, IPv6 literals), query-parameter parsing and re-serialization.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "axmol/network/Uri.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::string_view s(reinterpret_cast<const char*>(data), size);
    auto uri = ax::network::Uri::parse(s);
    if (uri.isValid())
    {
        (void)uri.getQueryParams();
        auto str = uri.toString();
        auto again = ax::network::Uri::parse(str);
        (void)again.isValid();
    }
    return 0;
}
