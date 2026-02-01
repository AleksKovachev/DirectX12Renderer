#include "Common.hlsli"

static const uint OUTPUT_ALBEDO_FACE = 0u;
static const uint OUTPUT_ALBEDO_RANDOM_COLORS = 1u;
static const uint OUTPUT_ALBEDO_DISCO_MODE = 2u;
static const uint OUTPUT_ALBEDO_SHADOW_OVERLAY_DEBUG = 3u;
static const uint OUTPUT_ALBEDO_CHECKER_TEXTURE = 4u;
static const uint OUTPUT_ALBEDO_GRID_TEXTURE = 5u;

static const uint SHADE_MODE_LIT = 0u;
static const uint SHADE_MODE_UNLIT = 1u;

Texture2D shadowMap : register( t0 );
SamplerComparisonState shadowSampler : register( s0 );

cbuffer RootConstants : register( b1 ) {
    int frameIdx;
};

// Not set as root constants as these don't change every frame - not worth taking the slots.
cbuffer SceneData : register( b2 ) {
    float4 geomColor;
    float4 textureColorA;
    float4 textureColorB;
    uint outputAlbedo;
    uint discoSpeed;
    uint shadeMode;
    float gridTiling;
    float gridProportionsX;
    float gridProportionsY;
    uint2 _pad;
};

cbuffer LightingCB : register( b3 ) {
    float4 dirLightColor;
    float3 lightDirVS;
    float lightIntensity;
    float specStrength; // Currently global. Needs to be per-mesh (material prop).
    float shadowBiasMultiplier;
    float ambientIntensity;
    float _pad2;
};

cbuffer LightMatrices : register( b4 ) {
    row_major float4x4 lightViewProjMatrix;
}

float3 GetAlbedoColor( uint primID, VSOutput_Faces inputVertex ) {
    float3 redColor = float3( 0.84f, 0.41f, 0.29f );
    float3 blueColor = float3( 0.21f, 0.5f, 0.73f );
    float3 purpleColor = float3( 0.49f, 0.52f, 0.97f );
    float3 yellowColor = float3( 1.f, 0.99f, 0.57f );
    float3 orangeColor = float3( 0.94f, 0.53f, 0.31f );
    float3 pinkColor = float3( 0.94f, 0.53f, 0.75f );
    float3 whiteColor = float3( 1.f, 1.f, 1.f );

    if ( outputAlbedo == OUTPUT_ALBEDO_FACE ) {
        return geomColor.xyz;
    } else if ( outputAlbedo == OUTPUT_ALBEDO_RANDOM_COLORS ) {
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
    } else if ( outputAlbedo == OUTPUT_ALBEDO_DISCO_MODE ) {
        if ( frameIdx % discoSpeed <= discoSpeed / 2 ) {
            return textureColorA.xyz;
        } else {
            return textureColorB.xyz;
        }
    } else if ( outputAlbedo == OUTPUT_ALBEDO_CHECKER_TEXTURE ) {
        float2 grid = inputVertex.worldPos.xz * gridTiling;
        // frac(x) extracts the fractional part of a float. Ex. frac( 3.14 ) = 0.14
        float checker = ( frac( grid.x ) > gridProportionsX ) ^ ( frac( grid.y ) > gridProportionsY );
        if ( checker != 0.f ) {
            return textureColorA.xyz;
        } else {
            return textureColorB.xyz;
        }
    } else if ( outputAlbedo == OUTPUT_ALBEDO_GRID_TEXTURE ) {
        float2 grid = frac( inputVertex.worldPos.xz * gridTiling );

        if ( grid.x < gridProportionsX || grid.y < gridProportionsY ) {
            return textureColorA.xyz;
        }
        return textureColorB.xyz;
    } else {
        return float3( 1, 0, 0 ); // Should never get here. Solid red as error!
    }
}

float ShadowFactor( float4 worldPos ) {
    float4 lightSpacePos = mul( worldPos, lightViewProjMatrix );
    lightSpacePos.xyz /= lightSpacePos.w;

    // Transform to screen space [0, 1] for texture lookup.
    float2 uv = float2( lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f );
    float depth = lightSpacePos.z; // Already in [0,1] for orthographic.

    // Everything outside the shadow map should be Lit.
    if ( uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1 ) {
        return 1.f;
    }
    float baseBias = 0.000001f;
    // Compare with shadow map. 0 = shadowed, 1 = lit.
    return shadowMap.SampleCmpLevelZero(
        shadowSampler, uv, depth + ( baseBias * shadowBiasMultiplier ) );
}

float4 PSMain( VSOutput_Faces inputVertex, uint primID : SV_PrimitiveID ) : SV_TARGET {
    float3 albedo = GetAlbedoColor( primID, inputVertex );

    // Don't calculate lighting in "Unlit" shade mode.
    // More performant with a separate PSO than having this "if" check foreach vertex.
    if ( shadeMode == SHADE_MODE_UNLIT ) {
        return float4( albedo, 1.f );
    }

    float3 surfaceNormal = normalize( inputVertex.normal );
    // Direction from surface to light (view space).
    float3 lightDir = normalize( -lightDirVS );

    // Lambertian diffuse term.
    float diffuseFactor = saturate( dot( surfaceNormal, lightDir ) );

    // View direction (camera at origin in view space).
    float3 viewDir = normalize( -inputVertex.worldPos );
    float3 halfDir = normalize( lightDir + viewDir );

    // Blinn-Phong.
    float specularFactor = pow( saturate( dot( surfaceNormal, halfDir ) ), specStrength );

    float3 diffuseLighting = albedo * dirLightColor.xyz * diffuseFactor;
    float3 specularLighting = dirLightColor.xyz * specularFactor;

    float shadow = ShadowFactor( float4( inputVertex.worldPos, 1 ) );

    if ( outputAlbedo == OUTPUT_ALBEDO_SHADOW_OVERLAY_DEBUG ) {
        float4 lightSpacePos = mul( float4( inputVertex.worldPos, 1 ), lightViewProjMatrix );
        lightSpacePos.xyz /= lightSpacePos.w;
        if ( lightSpacePos.z > shadow + 0.001 ) {
            return textureColorB; // In shadow
        }
        return textureColorA; // Lit
    }

    // Acurate ambient lighting (Sky-ground) without ambient occlusion.
    //float upFactor = clamp( dot( normal, upDirection ), 0.0, 1.0 );
    //float3 ambient = mix( groundColor, skyColor, upFactor ) * albedo;

    float3 ambient = albedo * ambientIntensity;
    float3 finalColor = ambient + lightIntensity * ( diffuseLighting + specularLighting ) * shadow;

    return float4( finalColor, 1.f );
}