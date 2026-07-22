#pragma once

#ifdef _WIN32
#include <d3d9types.h>
#else

#include "xplatform.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

using D3DCOLOR = std::uint32_t;

struct D3DVECTOR
{
    float x;
    float y;
    float z;
};

struct D3DCOLORVALUE
{
    float r;
    float g;
    float b;
    float a;
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#endif
struct D3DMATRIX
{
    union
    {
        struct
        {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

inline D3DCOLOR D3DCOLOR_COLORVALUE(float red, float green, float blue, float alpha)
{
    const auto component = [](float value) -> std::uint32_t {
        return static_cast<std::uint32_t>(
            std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };

    return (component(alpha) << 24u) |
           (component(red) << 16u) |
           (component(green) << 8u) |
           component(blue);
}

#endif
