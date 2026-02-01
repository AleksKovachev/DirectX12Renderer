#include "Common.hlsli"

cbuffer RootConstants : register( b1 ) {
    uint packedColor;
};

float4 PSEdges( VSOutput_Edges_Verts input ) : SV_Target {
    return UnpackColor( packedColor );
}
