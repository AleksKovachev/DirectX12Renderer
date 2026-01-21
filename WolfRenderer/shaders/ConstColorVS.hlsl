#include "Common.hlsli"

cbuffer TransformCB : register( b0 ) {
    row_major float4x4 WorldView;
    row_major float4x4 Projection;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

VSOutput_Faces VSMain( VSInput inputVertex ) {
    VSOutput_Faces output;

    // Geometry position offset.
    float4 worldPos = mul( WorldView, float4( inputVertex.position, 1.f ) );

    output.position = mul( Projection, worldPos );
    output.worldPos = worldPos.xyz;

    float3x3 normalMatrix = (float3x3) WorldView;
    output.normal = normalize( mul( inputVertex.normal, normalMatrix ) );

    return output;
};
