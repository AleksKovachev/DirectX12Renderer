struct PSInput {
    float4 position : SV_POSITION;
    float2 localPos : TEXCOORD0; // [-1, 1] quad space
};
