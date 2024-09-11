// Copyright 2016-2020 Cadic AB. All Rights Reserved.
// @Author	Fredrik Lindh [Temaran] (temaran@gmail.com) {https://github.com/Temaran}
///////////////////////////////////////////////////////////////////////////////////////

#include "PixelShaderExample.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Runtime/RenderCore/Public/PixelShaderUtils.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "UniformBuffer.h"

/************************************************************************/
/* Simple static vertex buffer.                                         */
/************************************************************************/
class FSimpleScreenVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(6);

		Vertices[0].Position = FVector4f(-1, 1, 0, 1);
		Vertices[0].UV = FVector2f(0, 0);

		Vertices[1].Position = FVector4f(1, 1, 0, 1);
		Vertices[1].UV = FVector2f(1, 0);

		Vertices[2].Position = FVector4f(-1, -1, 0, 1);
		Vertices[2].UV = FVector2f(0, 1);

		Vertices[3].Position = FVector4f(1, -1, 0, 1);
		Vertices[3].UV = FVector2f(1, 1);

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(TEXT("FRHIResourceCreateInfo"),  & Vertices);
		VertexBufferRHI = RHICmdList.CreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};
TGlobalResource<FSimpleScreenVertexBuffer> GSimpleScreenVertexBuffer;

/************************************************************************/
/* A simple passthrough vertexshader that we will use.                  */
/************************************************************************/
class FSimplePassThroughVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSimplePassThroughVS);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FSimplePassThroughVS() { }
	FSimplePassThroughVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)	{ }
};

/**********************************************************************************************/
/* This class carries our parameter declarations and acts as the bridge between cpp and HLSL. */
/**********************************************************************************************/
class FPixelShaderExamplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPixelShaderExamplePS);
	SHADER_USE_PARAMETER_STRUCT(FPixelShaderExamplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D<uint>, ComputeShaderOutput)
		SHADER_PARAMETER(FVector4f, StartColor)
		SHADER_PARAMETER(FVector4f, EndColor)
		SHADER_PARAMETER(FVector2f, TextureSize) // Metal doesn't support GetDimensions(), so we send in this data via our parameters.
		SHADER_PARAMETER(float, BlendFactor)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// This will tell the engine to create the shader and where the shader entry point is.
//                           ShaderType                            ShaderPath                Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FSimplePassThroughVS, "/TutorialShaders/Private/PixelShader.usf", "MainVertexShader", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FPixelShaderExamplePS, "/TutorialShaders/Private/PixelShader.usf", "MainPixelShader", SF_Pixel);

void FPixelShaderExample::DrawToRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, FTextureRHIRef ComputeShaderOutput)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_PixelShader); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_Pixel); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FRHIRenderPassInfo RenderPassInfo(DrawParameters.RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("ShaderPlugin_OutputToRenderTarget"));

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FSimplePassThroughVS> VertexShader(ShaderMap);
	TShaderMapRef<FPixelShaderExamplePS> PixelShader(ShaderMap);
		
	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
	
	// Setup the pixel shader
	FPixelShaderExamplePS::FParameters PassParameters; 
	PassParameters.ComputeShaderOutput = ComputeShaderOutput;
	PassParameters.StartColor = FVector4f(DrawParameters.StartColor.R, DrawParameters.StartColor.G, DrawParameters.StartColor.B, DrawParameters.StartColor.A) / 255.0f;
	PassParameters.EndColor = FVector4f(DrawParameters.EndColor.R, DrawParameters.EndColor.G, DrawParameters.EndColor.B, DrawParameters.EndColor.A) / 255.0f;
	PassParameters.TextureSize = FVector2f(DrawParameters.GetRenderTargetSize().X, DrawParameters.GetRenderTargetSize().Y);
	PassParameters.BlendFactor = DrawParameters.ComputeShaderBlend;	
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters);
	
	// Draw
	RHICmdList.SetStreamSource(0, GSimpleScreenVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawPrimitive(0, 2, 1);
	RHICmdList.EndRenderPass();
}
