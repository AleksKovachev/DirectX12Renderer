#include "Common.hlsli"

cbuffer TransformCB : register( b1 ) {
    row_major float4x4 WorldView;
    row_major float4x4 Projection;
};

struct VSInput {
    float3 position : POSITION;
};

PSInput VSMain( VSInput inputVertex ) {
    PSInput output;

    // Geometry position offset.
    float4 pos = float4( inputVertex.position, 1.0f );
    pos = mul( WorldView, pos );
    output.position = mul( Projection, pos );

    return output;
};
