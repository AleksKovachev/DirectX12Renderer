#include "Common.hlsli"

cbuffer RootConstants : register( b1 ) {
    int frameIdx;
};

cbuffer SceneData : register( b2 ) {
    uint geomColorPacked;
    bool useRandomColors;
    bool disco;
    uint discoSpeed;
    uint shadeMode; // 0 = Lit, 1 = Unlit
    uint3 _pad0;
};

cbuffer LightingCB : register( b3 ) {
    float3 lightDirVS;
    uint dirLightColorPacked;
    float lightIntensity;
    float specStrength; // Currently global. Needs to be per-mesh (material prop).
    float2 _pad1;
};

float3 GetAlbedoColor( uint primID ) {
    float3 redColor = float3( 0.84f, 0.41f, 0.29f );
    float3 blueColor = float3( 0.21f, 0.5f, 0.73f );
    float3 purpleColor = float3( 0.49f, 0.52f, 0.97f );
    float3 yellowColor = float3( 1.f, 0.99f, 0.57f );
    float3 orangeColor = float3( 0.94f, 0.53f, 0.31f );
    float3 pinkColor = float3( 0.94f, 0.53f, 0.75f );
    float3 whiteColor = float3( 1.f, 1.f, 1.f );

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
        return UnpackColor( geomColorPacked ).xyz;
    }
}

float4 PSMain( VSOutput_Faces input, uint primID : SV_PrimitiveID ) : SV_TARGET {
    float3 albedo = GetAlbedoColor( primID );

    // Don't calculate lighting in "Unlit" shade mode.
    if ( shadeMode == 1 ) {
        return float4( albedo, 1.f );
    }

    float3 surfaceNormal = normalize( input.normal );
    // Direction from surface to light (view space).
    float3 lightDir = normalize( -lightDirVS );

    // Lambertian diffuse term.
    float diffuseFactor = saturate( dot( surfaceNormal, lightDir ) );

    // View direction (camera at origin in view space).
    float3 viewDir = normalize( -input.worldPos );
    float3 halfDir = normalize( lightDir + viewDir );

    // Blinn-Phong.
    float specularFactor = pow( saturate( dot( surfaceNormal, halfDir ) ), specStrength );

    float3 dirLightColor = UnpackColor( dirLightColorPacked ).xyz;

    float3 diffuseLighting = albedo * dirLightColor * diffuseFactor;
    float3 specularLighting = dirLightColor * specularFactor;

    float3 finalColor = lightIntensity * ( diffuseLighting + specularLighting );

    return float4( finalColor, 1.f );
}