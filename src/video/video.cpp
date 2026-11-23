#include "video/video.h"

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
