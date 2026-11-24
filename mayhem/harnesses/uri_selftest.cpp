// Known-answer functional test for ax::network::Uri (axmol/network/Uri.cpp), built with normal flags.
#include <cstdio>
#include <string>
#include <string_view>

#include "axmol/network/Uri.h"

using ax::network::Uri;
static int g_pass, g_fail;
static void check(const char* name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? g_pass : g_fail)++;
}

int main()
{
    auto u = Uri::parse("https://user:pw@www.example.com:8443/a/b.html?x=1&y=two#frag");
    check("full_valid", u.isValid());
    check("full_secure", u.isSecure());
    check("full_scheme", u.getScheme() == "https");
    check("full_user", u.getUserName() == "user");
    check("full_password", u.getPassword() == "pw");
    check("full_host", u.getHost() == "www.example.com");
    check("full_port", u.getPort() == 8443 && u.isCustomPort());
    check("full_path", u.getPath() == "/a/b.html");
    check("full_query", u.getQuery() == "x=1&y=two");
    check("full_fragment", u.getFragment() == "frag");
    auto& qp = u.getQueryParams();
    check("full_query_params", qp.size() == 2 && qp[0].first == "x" && qp[0].second == "1" &&
                                   qp[1].first == "y" && qp[1].second == "two");

    auto d = Uri::parse("http://example.org/");
    check("default_port_http", d.isValid() && d.getPort() == 80 && !d.isCustomPort() && !d.isSecure());

    auto v6 = Uri::parse("http://[::1]:8080/x");
    check("ipv6_host", v6.isValid() && v6.getHost() == "[::1]" && v6.getHostName() == "::1" && v6.getPort() == 8080);

    check("empty_invalid", !Uri::parse("").isValid());
    check("bad_ipv6_invalid", !Uri::parse("http://[::1/x").isValid());
    check("bad_port_invalid", !Uri::parse("http://host:abc/").isValid());
    check("bad_scheme_invalid", !Uri::parse("1http://x/").isValid());

    auto r = Uri::parse(u.toString());
    check("roundtrip", r.isValid() && r.getHost() == "www.example.com" && r.getPath() == "/a/b.html" &&
                           r.getQuery() == "x=1&y=two" && r.getFragment() == "frag");
    printf("RESULT passed=%d failed=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
