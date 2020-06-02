#define MAX_LIGHT_COUNT 12

float4 GScreenInfo;
float4 GLightPos[MAX_LIGHT_COUNT];
float4 GLightColor[MAX_LIGHT_COUNT];
//float4 GLightRadius[MAX_LIGHT_COUNT / 4]; // GLightPos 의 x,y 를 제외한 z값으로 사용하기로 하자
float4 GAmbientColor;

struct PS_IN
{
	float2 Tx0 : TEXCOORD0;
};

struct PS_OUT
{
	float4 Color : COLOR0;
};

static float2 Pos;

float3 ComputeLight(int Index)
{
	//float4 LightPosVec = GLightPos[Index];
	//int ElemIndex = (Index % 2) * 2;
	//float2 LightPos = float2(LightPosVec[ElemIndex], LightPosVec[ElemIndex + 1]);
	
	float2 LightPos = float2(GLightPos[Index].x, GLightPos[Index].y);

	float CircleDist = distance(Pos, LightPos);

	//float Radius = GLightRadius[Index / 4][Index % 4];
	float Radius = GLightPos[Index].z;

	float Attenuation = saturate(GLightColor[Index].a) * pow(saturate(1.0f - pow(CircleDist / Radius, 4)), 2) / (CircleDist * CircleDist / (GLightColor[Index].a + 1) + 1.0f);
	//float Attenuation = GLightColor[Index].a  * pow(saturate(1 - pow(CircleDist / Radius, 4)), 2) / (CircleDist * CircleDist + 1);
	return GLightColor[Index].rgb * Attenuation;
}

PS_OUT main( PS_IN In ) : COLOR
{
	PS_OUT Out = (PS_OUT)0;
	Pos = In.Tx0 * float2(GScreenInfo.b, GScreenInfo.a) + float2(GScreenInfo.r, GScreenInfo.g);

	float4 Result = GAmbientColor;
	

	for(int i = 0; i < MAX_LIGHT_COUNT; ++i)
	{
		//if (i < GAmbientColor.a) Result.rgb += ComputeLight(i);
		//if ( GLightColor[i].a != 0.0f ) Result.rgb += ComputeLight(i);
		//Result.rgb += ComputeLight(i);
		Result.rgb += ComputeLight(i);
	}

	Result.a = 1.0f;
	Out.Color = Result;

	return Out;
}