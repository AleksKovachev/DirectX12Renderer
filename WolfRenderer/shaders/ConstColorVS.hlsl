#include "Common.hlsli"

struct VSInput
{
    float2 position : POSITION;
};

PSInput VSMain(VSInput input)
{
    return PSInput(float4(input.position, 0.0f, 1.0f));
}
