#include "Common.hlsli"

cbuffer RootConstants : register( b0 ) {
    int frameIdx;
};

float4 PSMain( PSInput input ) : SV_TARGET {
    float4 redColor = float4( 1.0, 0.0, 0.0, 1.0 );
    float4 purpleColor = float4( 0.5, 0.0, 0.5, 1.0 );

    if ( frameIdx % 800 <= 400 ) {
        return redColor;
    } else {
        return purpleColor;
    }
}