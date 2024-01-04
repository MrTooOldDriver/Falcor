Texture2D<float4>   gNormalFrame, gShadowFrame;  // Last and current frame inputs

float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
    uint2 pixelPos = (uint2)pos.xy; // Where is this pixel on screen?
    float4 normColor = gNormalFrame[pixelPos];
    float4 shadowColor = gShadowFrame[pixelPos];

    float4 black = float4(0.0f, 0.0f, 0.0f, 1.f);
    float epsilon = 0.0001f;

    float4 return_colour = float4(0.0f, 0.0f, 0.0f, 1.f);

    if (length(shadowColor - black) > epsilon) {
        return_colour = normColor - shadowColor;
    } else {
        return_colour = normColor;
    }
    return return_colour;
}
