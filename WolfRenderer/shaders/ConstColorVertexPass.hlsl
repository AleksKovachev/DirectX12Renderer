#include "Common.hlsli"

float4 PSMain( PSInput input ) : SV_Target {
    // Convert quad to circle
    float dist = length( input.localPos );

    // Hard cutoff (perfect circle)
    if ( dist > 1.0f )
        discard;

    // Black vertices.
    float4 color = { 0.9f, 0.5f, 0.25f, 0.1f };

    return color;
}
