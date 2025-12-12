#include "Common.hlsli"

struct VSInput {
    float2 position : POSITION;
};

PSInput VSMain( VSInput inputVertex ) {
    PSInput output;

    // Geometry position offset.
    float2 pos = inputVertex.position;
    float2 offset = { pos.x + clamp( offsetX, -1.f, 1.f ), pos.y + clamp( -offsetY, -1.f, 1.f ) };
    output.position = float4( offset, 0.0f, 1.0f );

    return output;

    // Geometry rotation.
    // float STEP = 0.0001f;
    // float angle = (float)frameIdx * STEP;
    // float cosA = cos( angle );
    // float sinA = sin( angle );

    // float2 pos = inputVertex.position;

    // float2 rotated = { pos.x * cosA - pos.y * sinA, pos.x * sinA + pos.y * cosA };
    // output.position = float4( rotated, 0.0f, 1.0f );
    // return output;
};
