#include <windows.h>

#if !defined(WINVER) || WINVER != 0x0601
#error "workflow must target Windows 7 through WINVER=0x0601"
#endif

#if !defined(_WIN32_WINNT) || _WIN32_WINNT != 0x0601
#error "workflow must target Windows 7 through _WIN32_WINNT=0x0601"
#endif

#if !defined(NTDDI_VERSION) || NTDDI_VERSION != 0x06010000
#error "workflow must target Windows 7 SP1 through NTDDI_VERSION=NTDDI_WIN7SP1"
#endif

int main() {
    return 0;
}
