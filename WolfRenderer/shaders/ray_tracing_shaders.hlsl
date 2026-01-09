RaytracingAccelerationStructure sceneBVHAccStruct : register( t0 );
RWTexture2D<float4> frameTexture : register( u0 );

struct RayPayload {
    float4 pixelColor;
};

[shader("raygeneration")]
void rayGen() {
    // Render resolution. Z channel is depth.
    // DispatchRaysDimensions() gets the dimensions specified in DispatchRays call.
    uint2 res = DispatchRaysDimensions().xy;
    float width = float( res.x );
    float height = float( res.y );

    // RayDesc struct is built-in HLSL struct for ray description.
    RayDesc cameraRay;
    cameraRay.Origin = float3( 0.f, 0.f, 0.f );

    // Get raster coords of the current pixel being processed.
    uint2 pixelRasterCoords = DispatchRaysIndex().xy;

    // Fill the frame with blue color on hit.
    // frameTexture[pixelRasterCoords] = float4( 0.0, 0.0, 1.0, 1.0 );

    float x = pixelRasterCoords.x;
    float y = pixelRasterCoords.y;

    // Create a gradient represented by the pixel raster coordinates.
    // float rasterR = ( x % 100 ) / 255.f;
    // float rasterG = ( y % 100 ) / 255.f;
    // cameraRay.Direction = float3( rasterR, rasterG, 0.f );
    // frameTexture[pixelRasterCoords] = float4( cameraRay.Direction, 1.f );

    // Center the coordinates to the pixel.
    x += 0.5f;
    y += 0.5f;

    // Normalize to NDC [0, 1].
    x /= width;
    y /= height;

    // Now the gradient is one - for the whole screen, not per pixel.
    // cameraRay.Direction = float3( x, y, 0.f );
    // frameTexture[pixelRasterCoords] = float4( cameraRay.Direction, 1.f );

    // NDC to screen space [-1, 1].
    x = ( 2.f * x ) - 1.f;
    y = 1.f - ( 2.f * y ); // Flip Y axis.

    // Put the blue channel (black color) in the center. Red - horizontal, Green - vertical.
    // float screenR = abs( x );
    // float screenG = abs( y );
    // cameraRay.Direction = float3( screenR, screenG, 0.f );
    // frameTexture[pixelRasterCoords] = float4( cameraRay.Direction, 1.f );

    x *= width / height; // Adjust for aspect ratio.

    // float aspectR = abs( x );
    // float aspectG = abs( y );
    // cameraRay.Direction = float3( aspectR, aspectG, 0.f );
    // frameTexture[pixelRasterCoords] = float4( cameraRay.Direction, 1.f );

    // Set the Z component to -1 to point into the scene.
    float3 rayDirection = float3( x, y, -1.f );

    // float rayDirR = abs( rayDirection.x );
    // float rayDirG = abs( rayDirection.y );
    // float rayDirB = abs( rayDirection.z );
    // cameraRay.Direction = float3( rayDirR, rayDirG, rayDirB );
    // frameTexture[pixelRasterCoords] = float4( cameraRay.Direction, 1.f );

    float3 rayDirectionNormalized = normalize( rayDirection );

    // cameraRay.Direction = rayDirectionNormalized;

    // float r = abs( cameraRay.Direction.x );
    // float g = abs( cameraRay.Direction.y );
    // float b = abs( cameraRay.Direction.z );

    // frameTexture[pixelRasterCoords] = float4( r, g, b, 1.f );

    cameraRay.Direction = rayDirectionNormalized;

    /* T is the parameter with which the points along the line that
     * is passing through the ray are being calculated.
     * DXR ignores intersections with values outside the
     * given range defined by the parametric value T.*/
    cameraRay.TMin = 0.001;
    cameraRay.TMax = 10000.0;

    RayPayload rayPayload;
    rayPayload.pixelColor = float4( 0.f, 0.f, 0.f, 1.f );

    TraceRay(
        sceneBVHAccStruct,
        RAY_FLAG_NONE,
        0xFF,
        0,
        1,
        0,
        cameraRay,
        rayPayload
    );

    frameTexture[pixelRasterCoords] = rayPayload.pixelColor;
}

[shader("miss")]
void miss(inout RayPayload payload) {
    // Fill the frame with green color on miss.
    payload.pixelColor = float4(0.0, 1.0, 0.0, 1.0);
}

[shader("closesthit")]
void closestHit(
    inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr
) {
    // Fill the frame with red color on hit.
    payload.pixelColor = float4( 1.0, 0.0, 0.0, 1.0 );
}