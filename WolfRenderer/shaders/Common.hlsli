// Solid triangle rendering.
struct VSOutput_Faces {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
};

// Wireframe.
struct VSOutput_Edges_Verts {
    float4 position : SV_POSITION;
};

struct GSOutput_Verts {
    float4 position : SV_POSITION;
    float2 localPos : TEXCOORD0; // [-1, 1] quad space
};


/// Unpack an 8-bit color, packed in a uint's bits.
float4 UnpackColor( uint packed ) {
    return float4(
        ( packed & 0xFF ), // R
        ( packed >> 8 ) & 0xFF, // G
        ( packed >> 16 ) & 0xFF, // B
        ( packed >> 24 ) & 0xFF // A
    ) / 255.0f;
}
