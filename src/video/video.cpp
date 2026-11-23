#include "video/video.h"

#include <algorithm>
#include <cctype>

namespace video {

std::unique_ptr<Video> create_null();
std::unique_ptr<Video> create_software();

const char* backend_name(Backend b) {
    switch (b) {
        case Backend::Null: return "null";
        case Backend::Software: return "software";
        case Backend::GLES: return "gles";
        case Backend::Vulkan: return "vulkan";
    }
    return "?";
}

const char* gpu_family_name(GpuFamily f) {
    switch (f) {
        case GpuFamily::Unknown: return "desconhecida";
        case GpuFamily::Software: return "software";
        case GpuFamily::Mali: return "mali";
        case GpuFamily::Adreno: return "adreno";
        case GpuFamily::PowerVR: return "powervr";
        case GpuFamily::Apple: return "apple";
        case GpuFamily::Intel: return "intel";
        case GpuFamily::Nvidia: return "nvidia";
        case GpuFamily::Amd: return "amd";
    }
    return "?";
}

GpuFamily gpu_family_detect(uint32_t vendorId, const std::string& renderer) {
    switch (vendorId) {
        case 0x13B5: return GpuFamily::Mali;
        case 0x5143: return GpuFamily::Adreno;
        case 0x1010: return GpuFamily::PowerVR;
        case 0x106B: return GpuFamily::Apple;
        case 0x8086: return GpuFamily::Intel;
        case 0x10DE: return GpuFamily::Nvidia;
        case 0x1002: return GpuFamily::Amd;
        default: break;
    }
    std::string r = renderer;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (r.find("mali") != std::string::npos) return GpuFamily::Mali;
    if (r.find("adreno") != std::string::npos) return GpuFamily::Adreno;
    if (r.find("powervr") != std::string::npos) return GpuFamily::PowerVR;
    if (r.find("apple") != std::string::npos) return GpuFamily::Apple;
    if (r.find("intel") != std::string::npos) return GpuFamily::Intel;
    if (r.find("nvidia") != std::string::npos || r.find("geforce") != std::string::npos) return GpuFamily::Nvidia;
    if (r.find("radeon") != std::string::npos || r.find("amd") != std::string::npos) return GpuFamily::Amd;
    return GpuFamily::Unknown;
}

std::unique_ptr<Video> create(Backend b, std::string* error) {
    switch (b) {
        case Backend::Null: return create_null();
        case Backend::Software: return create_software();
        case Backend::GLES:
        case Backend::Vulkan:
            if (error) *error = std::string("backend ") + backend_name(b) + " ainda nao implementado (veja docs/video.md)";
            return nullptr;
    }
    return nullptr;
}

} // namespace video
