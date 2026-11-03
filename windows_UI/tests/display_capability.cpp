#include "../src/WindowsDisplay.hpp"
#include <iostream>
int main() {
    const auto missing=spk::desktop::display_capability(L"round6-nonexistent-display");
    if(missing.known||missing.advanced_color_supported||missing.advanced_color_enabled||missing.bits_per_channel)return 1;
#ifdef _WIN32
    DISPLAY_DEVICEW display{};display.cb=sizeof(display);
    for(DWORD i=0;EnumDisplayDevicesW(nullptr,i,&display,0);++i) {
        if(!(display.StateFlags&DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))continue;
        const auto c=spk::desktop::display_capability(display.DeviceName);
        std::cout<<"active display "<<i<<": known="<<c.known<<", advanced-color-supported="<<c.advanced_color_supported
            <<", enabled="<<c.advanced_color_enabled<<", bits="<<c.bits_per_channel<<", Windows-status="<<c.error<<'\n';
    }
#endif
    std::cout<<"Unknown/invalid display never advertises HDR; application SDR remains explicit\n";
}
