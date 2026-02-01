#include "Common.hlsli"

cbuffer TransformCB : register( b0 ) {
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

VSOutput_Faces VSMain( VSInput inputVertex ) {
    VSOutput_Faces output;

    // Geometry position offset.
    float4 worldPos = mul( float4( inputVertex.position, 1.f ), World );

    // Transform to view space for projection.
    float4 viewPos = mul( worldPos, View );
    output.position = mul( viewPos, Projection );

    output.worldPos = worldPos.xyz;

    // Transform normal to world space.
    float3x3 normalMatrix = (float3x3)World;
    output.normal = normalize( mul( inputVertex.normal, normalMatrix ) );

    return output;
};
