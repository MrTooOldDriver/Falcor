Texture2D<uint> gShadowFrame;  // Half-resolution shadow texture
RWTexture2D<uint> gUpsampledShadowFrame;  // Full-resolution shadow texture

cbuffer PerFrameCB
{
    float shadowFactor;
}

[numthreads(16, 16, 1)]
void nearest_neighbour_upsample(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    // Calculate the corresponding position in the half-resolution texture
    float2 halfResPos = dispatchThreadID.xy / shadowFactor;

    // Sample the half-resolution texture
    uint shadowBitmask = gShadowFrame.Load(int3(halfResPos, 0));

    // Write the color to the full-resolution texture
    gUpsampledShadowFrame[dispatchThreadID.xy] = shadowBitmask;
}

[numthreads(16, 16, 1)]
void bilinear_upsample(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    // Calculate the corresponding position in the half-resolution texture
    float2 halfResPos = dispatchThreadID.xy / shadowFactor;

    // Get the four nearest pixels in the half-resolution texture
    uint2 topLeft = uint2(floor(halfResPos));
    uint2 topRight = topLeft + uint2(1, 0);
    uint2 bottomLeft = topLeft + uint2(0, 1);
    uint2 bottomRight = topLeft + uint2(1, 1);

    // Calculate the weights for each pixel
    float2 weights = frac(halfResPos);
    float topLeftWeight = (1 - weights.x) * (1 - weights.y);
    float topRightWeight = weights.x * (1 - weights.y);
    float bottomLeftWeight = (1 - weights.x) * weights.y;
    float bottomRightWeight = weights.x * weights.y;

    // Perform the bilinear interpolation
    uint shadowBitmask = uint(
        topLeftWeight * gShadowFrame.Load(int3(topLeft, 0)) +
        topRightWeight * gShadowFrame.Load(int3(topRight, 0)) +
        bottomLeftWeight * gShadowFrame.Load(int3(bottomLeft, 0)) +
        bottomRightWeight * gShadowFrame.Load(int3(bottomRight, 0))
    );

    // Write the result to the full-resolution texture
    gUpsampledShadowFrame[dispatchThreadID.xy] = shadowBitmask;
}
