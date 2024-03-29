float4x4 gWorld : WORLD;
float4x4 gWorldViewProj : WORLDVIEWPROJECTION;

// LIGHTING
float4x4 gWorldViewProj_Light;
float4x4 gBakedWorldViewProj_Light;
float3 gLightDirection = float3(-0.577f, -0.577f, 0.577f);
float gShadowMapBias = 0.0002f;
bool gUseBakedShadows = false;

// DIFFUSE COLOR
bool gUseDiffuseMap = true;
float4 gDiffuseColor = float4(1.f, 1.f, 1.f, 1.f);
Texture2D gDiffuseMap;

// SHADOWS
Texture2D gShadowMap;
Texture2D gBakedShadowMap;

SamplerState samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap; // or Mirror or Clamp or Border
    AddressV = Wrap; // or Mirror or Clamp or Border
};

SamplerComparisonState cmpSampler
{
	// sampler state
    Filter = COMPARISON_MIN_MAG_MIP_LINEAR;
    AddressU = MIRROR;
    AddressV = MIRROR;

	// sampler comparison state
    ComparisonFunc = LESS_EQUAL;
};

RasterizerState Solid
{
    FillMode = SOLID;
    CullMode = FRONT;
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 lPos : TEXCOORD1;
    float4 lPosBaked : TEXCOORD2;
};

DepthStencilState EnableDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
};

RasterizerState NoCulling
{
    CullMode = NONE;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;
	
	//Hint: Don't forget to project our position to light clip space and store it in lPos
    output.lPos = mul(float4(input.pos, 1.0f), gWorldViewProj_Light);
    output.lPosBaked = mul(float4(input.pos, 1.0f), gBakedWorldViewProj_Light);
	
	// Step 1:	convert position into float4 and multiply with matWorldViewProj
    output.pos = mul(float4(input.pos, 1.0f), gWorldViewProj);
	// Step 2:	rotate the normal: NO TRANSLATION
	//			this is achieved by clipping the 4x4 to a 3x3 matrix, 
	//			thus removing the postion row of the matrix
    output.normal = normalize(mul(input.normal, (float3x3) gWorld));
    output.texCoord = input.texCoord;
	
    return output;
}

float2 texOffset(int u, int v, int w, int h)
{
    return float2(u * 1.0f / w, v * 1.0f / h);
}

float EvaluateShadowMap(float4 lpos, Texture2D shadowMap)
{
	//re-homogenize position after interpolation
    lpos.xyz /= lpos.w;
 
    //if position is not visible to the light - dont illuminate it
    //results in hard light frustum
    if (lpos.x < -1.0f || lpos.x > 1.0f ||
        lpos.y < -1.0f || lpos.y > 1.0f ||
        lpos.z < 0.0f || lpos.z > 1.0f)
        return 1.0f;
 
    //transform clip space coords to texture space coords (-1:1 to 0:1)
    lpos.x = lpos.x / 2 + 0.5;
    lpos.y = lpos.y / -2 + 0.5;
 
	// Apply shadow map bias
    lpos.z -= gShadowMapBias;
 
	//PCF sampling for shadow map
    float sum = 0;
    float x, y;
    int width, height;
    shadowMap.GetDimensions(width, height);
    
    //perform PCF filtering on a 4 x 4 texel neighborhood
    for (y = -1.5f; y <= 1.5f; ++y)
        for (x = -1.5f; x <= 1.5f; ++x)
            sum += shadowMap.SampleCmpLevelZero(cmpSampler, lpos.xy + texOffset(x, y, width, height), lpos.z);
            
    return (sum / 16.0f) * 0.5f + 0.5f;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float shadowValue = EvaluateShadowMap(input.lPos, gShadowMap);
    if (gUseBakedShadows)
    {
        float bakedShadowValue = EvaluateShadowMap(input.lPosBaked, gBakedShadowMap);
        shadowValue = min(shadowValue, bakedShadowValue);
    }

    float4 diffuseColor = gDiffuseColor;
    if(gUseDiffuseMap)
        diffuseColor *= gDiffuseMap.Sample(samLinear, input.texCoord);

    float3 color_rgb = diffuseColor.rgb;
    float color_a = diffuseColor.a;
	
	//HalfLambert Diffuse :)
    float diffuseStrength = dot(input.normal, -gLightDirection);
    diffuseStrength = diffuseStrength * 0.5 + 0.5;
    diffuseStrength = saturate(diffuseStrength);
    color_rgb = color_rgb * diffuseStrength;

    return float4(color_rgb * shadowValue, color_a);
}

//--------------------------------------------------------------------------------------
// Technique
//--------------------------------------------------------------------------------------
technique11 Default
{
    pass P0
    {
        SetRasterizerState(NoCulling);
        SetDepthStencilState(EnableDepth, 0);

        SetVertexShader(CompileShader(vs_4_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, PS()));
    }
}

