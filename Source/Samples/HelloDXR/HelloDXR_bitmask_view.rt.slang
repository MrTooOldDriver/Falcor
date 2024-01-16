Texture2D<uint> gBitmaskIn;  // Last and current frame inputs

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
    uint2 pixelPos = (uint2)pos.xy; // Where is this pixel on screen?
    uint bitmask = gBitmaskIn[pixelPos]; // Get the bitmask for this pixel
    float4 colour = float4(1.0f,1.0f,1.0f,1.0f); // Default to white
    if (bitmask != 0) {
        colour = float4(0.0f,0.0f,0.0f,1.0f); // If the bitmask is 0, make this pixel transparent
    }
    return colour;
}
