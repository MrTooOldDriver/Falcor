Texture2D<uint> gBitmask;
RWTexture2DArray<float4> gShadowTexture;
RWTexture2D<float4> gDebugTexture2D;

[numthreads(1, 1, 1)]
void bitmask_to_shadow_texture(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    int2 bitmask_index = (dispatchThreadID.xy);
    uint bitmask = gBitmask[bitmask_index];
    uint mask = 1 << dispatchThreadID.z;
    // if ((bitmask & mask) == 0) {
    //     gShadowTexture[dispatchThreadID] = float4(1.0f, 1.0f, 1.0f, 1.0f);
    // }
    // else {
    //     gShadowTexture[dispatchThreadID] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    // }
    if (bitmask == 0) {
        gShadowTexture[dispatchThreadID] = float4(1.0f, 1.0f, 1.0f, 1.0f);
        gDebugTexture2D[dispatchThreadID.xy] = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else {
        gShadowTexture[dispatchThreadID] = float4(0.0f, 0.0f, 0.0f, 1.0f);
        gDebugTexture2D[dispatchThreadID.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

