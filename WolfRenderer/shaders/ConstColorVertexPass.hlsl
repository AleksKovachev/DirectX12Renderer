#include "Common.hlsli"

cbuffer RootConstants : register( b2 ) {
    uint packedColor;
};

float4 PSMain( GSOutput_Verts input ) : SV_Target {
    // Convert quad to circle
    float dist = length( input.localPos );

    // Hard cutoff (perfect circle)
    if ( dist > 1.f )
        discard;

    return UnpackColor( packedColor );
}
