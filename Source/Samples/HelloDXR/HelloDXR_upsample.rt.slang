Texture2D<float4> gShadowFrame;  // Half-resolution shadow texture
RWTexture2D<float4> gUpsampledShadowFrame;  // Full-resolution shadow texture

[numthreads(16, 16, 1)]
void nearest_neighbour_upsample(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    // Calculate the corresponding position in the half-resolution texture
    float2 halfResPos = dispatchThreadID.xy / 2;

    // Sample the half-resolution texture
    float4 shadowColor = gShadowFrame.Load(int3(halfResPos, 0));

    // Write the color to the full-resolution texture
    gUpsampledShadowFrame[dispatchThreadID.xy] = shadowColor;
}

[numthreads(16, 16, 1)]
void bi_linear_upsample(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // Calculate the corresponding position in the half-resolution texture
    float2 halfResPos = dispatchThreadID.xy / 2.0f;

    // Calculate the four nearest pixels in the half-resolution texture
    uint2 topLeft = uint2(halfResPos);
    uint2 topRight = topLeft + uint2(1, 0);
    uint2 bottomLeft = topLeft + uint2(0, 1);
    uint2 bottomRight = topLeft + uint2(1, 1);

    // Calculate the weights for each of the four pixels
    float2 weights = frac(halfResPos);
    float4 weightVector = float4((1 - weights.x) * (1 - weights.y), weights.x * (1 - weights.y), (1 - weights.x) * weights.y, weights.x * weights.y);

    // Sample the four nearest pixels in the half-resolution texture
    float4 topLeftColor = gShadowFrame.Load(int3(topLeft, 0));
    float4 topRightColor = gShadowFrame.Load(int3(topRight, 0));
    float4 bottomLeftColor = gShadowFrame.Load(int3(bottomLeft, 0));
    float4 bottomRightColor = gShadowFrame.Load(int3(bottomRight, 0));

    // Perform a weighted average of the four pixels
    float4 shadowColor = topLeftColor * weightVector.x + topRightColor * weightVector.y + bottomLeftColor * weightVector.z + bottomRightColor * weightVector.w;

    // Write the color to the full-resolution texture
    gUpsampledShadowFrame[dispatchThreadID.xy] = shadowColor;
}