#include "Common.hlsli"

cbuffer RootConstants : register( b0 ) {
    int frameIdx;
};

cbuffer SceneData : register( b2 ) {
    uint geomColorPacked;
    bool useRandomColors;
    bool disco;
    uint discoSpeed;
    //float _padding;
};

float4 PSMain( PSInput input, uint primID : SV_PrimitiveID ) : SV_TARGET {
    float4 redColor = float4( 1.0, 0.0, 0.0, 1.0 );
    float4 blueColor = float4( 0.0, 0.0, 1.0, 1.0 );
    float4 purpleColor = float4( 0.5, 0.0, 0.5, 1.0 );
    float4 yellowColor = float4( 1.0, 1.0, 0.0, 1.0 );
    float4 orangeColor = float4( 1.0, 0.5, 0.0, 1.0 );
    float4 pinkColor = float4( 1.0, 0.25, 0.5, 1.0 );
    float4 whiteColor = float4( 1.0, 1.0, 1.0, 1.0 );

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
        // Manual way.
        return UnpackColor( geomColorPacked );
    }
}