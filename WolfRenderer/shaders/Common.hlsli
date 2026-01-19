struct PSInput {
    float4 position : SV_POSITION;
    float2 localPos : TEXCOORD0; // [-1, 1] quad space
};

float4 UnpackColor( uint packed ) {
    return float4(
        ( packed & 0xFF ), // R
        ( packed >> 8 ) & 0xFF, // G
        ( packed >> 16 ) & 0xFF, // B
        ( packed >> 24 ) & 0xFF // A
    ) / 255.0f;
}
