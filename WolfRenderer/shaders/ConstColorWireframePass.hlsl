#include "Common.hlsli"

cbuffer RootConstants : register( b4 ) {
    uint packedColor;
};

float4 PSMain( PSInput input ) : SV_Target {
    return UnpackColor( packedColor );
}
