#include "Common.hlsli"

cbuffer RootConstants : register( b0 ) {
    int frameIdx;
};

cbuffer SceneData : register( b2 ) {
    uint geomColorPacked;
    bool useRandomColors;
    bool disco;
    uint discoSpeed;
};

float4 PSMain( PSInput input, uint primID : SV_PrimitiveID ) : SV_TARGET {
    float4 redColor    = float4( 0.84f, 0.41f, 0.29f, 1.f );
    float4 blueColor   = float4( 0.21f, 0.5f,  0.73f, 1.f );
    float4 purpleColor = float4( 0.49f, 0.52f, 0.97f, 1.f );
    float4 yellowColor = float4( 1.f,   0.99f, 0.57f, 1.f );
    float4 orangeColor = float4( 0.94f, 0.53f, 0.31f, 1.f );
    float4 pinkColor   = float4( 0.94f, 0.53f, 0.75f, 1.f );
    float4 whiteColor  = float4( 1.f,   1.f,   1.f,   1.f );

    if ( useRandomColors ) {
        if ( primID % 6 == 0 ) {
            return redColor;
        } else if ( primID % 6 == 1 ) {
            return purpleColor;
        } else if ( primID % 6 == 2 ) {
            return blueColor;
        } else if ( primID % 6 == 3 ) {
            return yellowColor;
        } else if ( primID % 6 == 4 ) {
            return orangeColor;
        } else if ( primID % 6 == 5 ) {
            return pinkColor;
        } else {
            return whiteColor;
        }
    } else if ( disco ) {
        if ( frameIdx % discoSpeed <= discoSpeed / 2 ) {
            return redColor;
        } else {
            return purpleColor;
        }
    } else {
        return UnpackColor( geomColorPacked );
    }
}