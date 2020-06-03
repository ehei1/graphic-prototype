#define MAX_LIGHT_COUNT 12

texture ForegroundTexture;  
float4 GBlurInfo;
// GBlurInfo.x - blur drection ( value 0.0f = X blur, value 1.0f = Y blur, value 2.0f = not blur )
// GBlurInfo.y - blur pixelwidth ( default 800.0f )
// GBlurInfo.z - blur pitch ( value 0.0f = PixelKernel13, value 1.0f = Pixelkernel9, value 2.0f = Pixelkernel5 )
// Foreground sampler  
sampler2D foreground = sampler_state {  
    Texture = (ForegroundTexture);  
    MinFilter = Point;  
    MagFilter = Point;  
    MipFilter = Point;
	//MinFilter = Linear;  
    //MagFilter = Linear;  
    //MipFilter = Linear;  
};  

// Pixel width in texels  
static float pixelWidth;  
  
static float PixelKernel13[13] =  
{  
    -6,  
    -5,  
    -4,  
    -3,  
    -2,  
    -1,  
     0,  
     1,  
     2,  
     3,  
     4,  
     5,  
     6,  
};

static const float BlurWeights13[13] =   
{  
    0.002216,  
    0.008764,  
    0.026995,  
    0.064759,  
    0.120985,  
    0.176033,  
    0.199471,  
    0.176033,  
    0.120985,  
    0.064759,  
    0.026995,  
    0.008764,  
    0.002216,  
};

struct PS_IN
{
	float2 Tx0 : TEXCOORD0;
	float4 Dif : COLOR0;
};

struct PS_OUT
{
	float4 Color : COLOR0;
};


PS_OUT main( PS_IN In ) : COLOR
{
	PS_OUT Out = (PS_OUT)0;
	float4 Result = 0;
	pixelWidth = 1.0f / GBlurInfo.y; // default 800
	// Apply surrounding pixels  
    //float4 color = 0;  
    float2 samp = In.Tx0;  

//	samp.y = In.Tx0.y;  
	
	if ( GBlurInfo.x == 2.0f ) {			// not blur
		Result = tex2D(foreground, samp.xy);
	}
	else {
		if ( GBlurInfo.x == 0.0f ) {
			samp.y = In.Tx0.y;  
			for (int i = 0; i < 13; i++) {  
				samp.x = In.Tx0.x + PixelKernel13[i] * pixelWidth;  
				Result += tex2D(foreground, samp.xy) *BlurWeights13[i];  
			} 
		}
		else if (  GBlurInfo.x == 1.0f ) {
			samp.x = In.Tx0.x;  
			for (int i = 0; i < 13; i++) {  
				samp.y = In.Tx0.y + PixelKernel13[i] * pixelWidth;  
				Result += tex2D(foreground, samp.xy) *BlurWeights13[i];  
			} 
		}
	}
	
	Out.Color = Result;

	return Out;
}

technique Blur
{
	pass P0
	{
		PixelShader = compile ps_2_0 main();
		BlendOp = ADD;
		SrcBlend = ONE;
		DestBlend = ZERO;
	}

	pass P1
	{
		PixelShader = compile ps_2_0 main();
		BlendOp = ADD;
		SrcBlend = ONE;
		DestBlend = ZERO;
	}

	pass P2
	{
		PixelShader = compile ps_2_0 main();
		BlendOp = ADD;
		SrcBlend = ONE;
		DestBlend = ZERO;
	}
}