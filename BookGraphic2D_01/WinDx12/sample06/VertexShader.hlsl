struct VSOutput {
	float4 Position : SV_POSITION;
	float2 UV0   : TEXCOORD;
};

VSOutput main(float2 pos : POSITION,
	float2 UV0 : TEXCOORD)
{
	VSOutput vsout = (VSOutput)0;
	vsout.Position = float4(pos, 0.0, 1.0);
	vsout.UV0 = UV0;
	return vsout;
}
