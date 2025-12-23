RWTexture2D<float4> frameTexture : register( u0 );

[shader("raygeneration")]
void rayGen() {
    uint2 pixelCoord = DispatchRaysIndex().xy;

    // Fill the frame with blue color on hit.
    frameTexture[pixelCoord] = float4( 0.0, 0.0, 1.0, 1.0 );

    // uint2 screenSize = DispatchRaysDimensions().xy;
    // float2 uv = (pixelCoord + 0.5) / screenSize;
    // float3 color = float3(uv, 0.5);
    // frameTexture[pixelCoord] = float4(color, 1.0);
}

struct RayPayload {
    float4 color;
};

[shader("miss")]
void miss(inout RayPayload payload) {
    // Fill the frame with green color on miss.
    payload.color = float4(0.0, 1.0, 0.0, 1.0);
}