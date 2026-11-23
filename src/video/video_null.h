#pragma once
#include "video/video.h"

namespace video {
// Backend que so grava as chamadas. Os testes do motor usam isto para conferir O QUE seria desenhado, sem GPU.
struct Call {
    enum Kind { Begin, Sprite, Rect, End } kind;
    SpriteDraw sprite;
    video::Rect rect;
    Call() : kind(Begin) {}
};
class NullVideo : public Video {
public:
    virtual const std::vector<Call>& calls() const = 0;
};
} // namespace video
