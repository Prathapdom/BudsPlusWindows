// Runs the fake earbuds from fakebuds.hpp on a fixed port, for trying the UI without hardware:
//   tools\fake-buds.exe [port]          (default 47100; stop with Ctrl+C)
//   set BUDSPLUS_FAKE=127.0.0.1:47100:5C587F   then start BudsPlus.exe
#include "../fakebuds.hpp"
#include <cstdio>

int wmain(int argc, wchar_t** argv) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    int port = argc > 1 ? _wtoi(argv[1]) : 47100;
    FakeBuds fake;
    if (!fake.start(port)) { printf("cannot listen on port %d\n", port); return 1; }
    printf("Fake CMF Buds 2 Plus listening on 127.0.0.1:%d\n", fake.port);
    for (;;) Sleep(1000);
}
