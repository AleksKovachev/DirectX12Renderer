#include "Common.hlsli"

cbuffer RootConstants : register( b5 ) {
    uint packedColor;
};

float4 PSMain( PSInput input ) : SV_Target {
    // Convert quad to circle
    float dist = length( input.localPos );

    // Hard cutoff (perfect circle)
    if ( dist > 1.0f )
        discard;

    return UnpackColor( packedColor );
}
