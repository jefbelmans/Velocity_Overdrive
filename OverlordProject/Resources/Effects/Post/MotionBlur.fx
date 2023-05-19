// SOURCE: https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-27-motion-blur-post-processing-effect

Texture2D gColorTexture;
Texture2D gDepthTexture;

float4x4 gInverseViewProj;
float4x4 gPreviousViewProj;

int gNumSamples = 3;

SamplerState samPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Mirror;
    AddressV = Mirror;
};

DepthStencilState depthEnabled
{
    DepthEnable = true;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
};

RasterizerState cullBack
{
    CullMode = Back;
};

//IN/OUT STRUCTS
//--------------
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;

};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD1;
};


//VERTEX SHADER
//-------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
	// Set the Position
    output.Position = float4(input.Position, 1.0f);
	// Set the TexCoord
    output.TexCoord = input.TexCoord;
	
    return output;
}


//PIXEL SHADER
//------------
float4 PS(PS_INPUT input) : SV_Target
{
    // Get the depth buffer value at this pixel.
    float depth = gDepthTexture.Sample(samPoint, input.TexCoord);
    
    // H is the viewport position at this pixel in the range -1 to 1.    
    float4 H = float4(input.TexCoord.x * 2 - 1, (1 - input.TexCoord.y) * 2 - 1, depth, 1);
    
    // Transform by the view-projection inverse.   
    float4 D = mul(H, gInverseViewProj);
    
    // Divide by w to get the world position.    
    float4 worldPos = float4(D.xyz / D.w, 1.f);
    
    // Current viewport position   
    float4 currentPos = H;
    
    // Use the world position, and transform by the previous view-projection matrix.
    float4 previousPos = mul(worldPos, gPreviousViewProj);
    
    // Convert to nonhomogeneous points [-1,1] by dividing by w.
    previousPos /= previousPos.w;
    
    // Use this frame's position and last frame's to compute the pixel velocity.
    float2 velocity = (currentPos - previousPos) * 0.5f; 
    
    float4 color = float4(0, 0, 0, 0);
    for(int i = 0; i < gNumSamples; ++i)
    { 
        // Sample the color buffer along the velocity vector and add to color
        color += gColorTexture.Sample(samPoint, input.TexCoord + velocity * i);
    }
    
    return color / gNumSamples;
}


//TECHNIQUE
//---------
technique11 Grayscale
{
    pass P0
    {
        SetDepthStencilState(depthEnabled, 0);
        SetRasterizerState(cullBack);

        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}