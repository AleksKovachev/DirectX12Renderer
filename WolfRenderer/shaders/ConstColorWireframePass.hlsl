#include "Common.hlsli"

cbuffer RootConstants : register( b1 ) {
    uint packedColor;
};

float4 PSMain( VSOutput_Edges_Verts input ) : SV_Target {
    return UnpackColor( packedColor );
}
