#include "Common.hlsli"

cbuffer ScreenCB : register( b3 ) {
    float2 viewportSize; // (width, height)
    float vertSize;
    float _padding;
};

struct GSOutput {
    float4 position : SV_POSITION;
    float2 localPos : TEXCOORD0; // [-1, 1] quad space
};

[maxvertexcount( 4 )]
void GSMain( point PSInput input[1], inout TriangleStream<GSOutput> outStream ) {
    float4 clipPosition = input[0].position;

    // Convert pixel size to clip-space offset.
    float2 pixelToClip = float2( 2.f / viewportSize.x, -2.f / viewportSize.y ) * vertSize;

    float2 offsets[4] = {
        float2( -1.f, -1.f ),
        float2( -1.f, 1.f ),
        float2( 1.f, -1.f ),
        float2( 1.f, 1.f )
    };

    GSOutput vertex;

    for ( uint i = 0; i < 4; ++i ) {
        vertex.position = clipPosition;
        vertex.position.xy += offsets[i] * pixelToClip * clipPosition.w;
        vertex.localPos = offsets[i]; // store quad-local coords

        outStream.Append( vertex );
    }

    outStream.RestartStrip();
};
