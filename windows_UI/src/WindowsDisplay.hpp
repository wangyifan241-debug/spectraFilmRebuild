#pragma once
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace spk::desktop {
// Advanced Color includes wide gamut SDR. These bits never assert that the
// application has an HDR swapchain or that a display supports a specific TRC.
struct DisplayCapability {
    bool known=false,advanced_color_supported=false,advanced_color_enabled=false;
    unsigned bits_per_channel=0;
    long error=0;
};
inline DisplayCapability display_capability(const std::wstring& device) {
    DisplayCapability out;
#ifdef _WIN32
    out.error=ERROR_NOT_FOUND;
    for(unsigned attempt=0;attempt<3;++attempt) {
        UINT32 paths=0,modes=0;
        LONG status=GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&paths,&modes);
        if(status!=ERROR_SUCCESS){out.error=status;return out;}
        std::vector<DISPLAYCONFIG_PATH_INFO> path(paths);
        std::vector<DISPLAYCONFIG_MODE_INFO> mode(modes);
        status=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&paths,path.data(),&modes,mode.data(),nullptr);
        if(status==ERROR_INSUFFICIENT_BUFFER)continue;
        if(status!=ERROR_SUCCESS){out.error=status;return out;}
        unsigned matches=0;
        for(UINT32 i=0;i<paths;++i) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME name{};
            name.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            name.header.size=sizeof(name);name.header.adapterId=path[i].sourceInfo.adapterId;
            name.header.id=path[i].sourceInfo.id;
            status=DisplayConfigGetDeviceInfo(&name.header);
            if(status!=ERROR_SUCCESS||device!=name.viewGdiDeviceName)continue;
            ++matches;
            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO info{};
            info.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
            info.header.size=sizeof(info);info.header.adapterId=path[i].targetInfo.adapterId;
            info.header.id=path[i].targetInfo.id;
            status=DisplayConfigGetDeviceInfo(&info.header);
            out.error=status;
            if(status==ERROR_SUCCESS) {
                out.known=true;out.advanced_color_supported=info.advancedColorSupported;
                out.advanced_color_enabled=info.advancedColorEnabled;
                out.bits_per_channel=info.bitsPerColorChannel;
            }
        }
        // A cloned source may drive unlike targets. Do not report one target
        // as the capability of the whole logical Qt screen.
        if(matches>1){out={};out.error=ERROR_NOT_SUPPORTED;}
        return out;
    }
    out.error=ERROR_INSUFFICIENT_BUFFER;
#endif
    return out;
}
}
