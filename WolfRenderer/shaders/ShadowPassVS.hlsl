cbuffer LightCB : register( b0 ) {
    row_major float4x4 lightViewProj;
};

cbuffer ObjectCB : register( b1 ) {
    row_major float4x4 worldMatrix;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput VSShadow( VSInput input ) {
    VSOutput output;
    float4 worldPos = mul( float4( input.position, 1.f ), worldMatrix );
    output.position = mul( worldPos, lightViewProj );
    return output;
}
