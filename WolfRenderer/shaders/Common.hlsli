struct PSInput {
    float4 position : SV_POSITION;
};

cbuffer RootConstants : register( b0 ) {
    int frameIdx;
    float offsetX;
    float offsetY;
};
