#include "Common.hlsli"

cbuffer TransformCB : register( b1 ) {
    float4x4 gTransform;
};

struct VSInput {
    float2 position : POSITION;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    float4 pos = float4( input.position, 0.0f, 1.0f );
    output.position = mul( pos, gTransform );
    return output;
}
