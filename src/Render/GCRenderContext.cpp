#include "pch.h"

GCRenderContext::GCRenderContext()
	: m_pGCRenderResources(nullptr),
	m_pCbCurrentViewProjInstance(nullptr),
	m_pCbLightPropertiesInstance(nullptr),
	m_pPostProcessingShader(nullptr),
	m_pPixelIdMappingShader(nullptr),
	m_isPixelIDMappingActivated(false),
	m_isDeferredLightPassActivated(false)
{
}

GCRenderContext::~GCRenderContext() {
	GC_DELETE(m_pGCRenderResources);
	GC_DELETE(m_pPostProcessingShader);
	GC_DELETE(m_pPixelIdMappingShader);

	GC_DELETE(m_pPostProcessingRtv);
	GC_DELETE(m_pPixelIdMappingBufferRtv);
	GC_DELETE(m_pPixelIdMappingDepthStencilBuffer);
}


bool GCRenderContext::Initialize(Window* pWindow, int renderWidth, int renderHeight, GCGraphics* pGraphics)
{
	if (!GC_CHECK_POINTERSNULL("Graphics Initialized with window sucessfully", "Can't initialize Graphics, Window is empty", pWindow))
		return false;

	m_pGCRenderResources = new GCRenderResources();
	m_pGCRenderResources->m_pGraphics = pGraphics;
	m_pGCRenderResources->m_renderWidth = renderWidth;
	m_pGCRenderResources->m_renderHeight = renderHeight;
	m_pGCRenderResources->m_pWindow = pWindow;

	InitDX12RenderPipeline();


	return true;
}

bool GCRenderContext::ResetCommandList() 
{
	HRESULT hr = m_pGCRenderResources->m_pCommandList->Reset(m_pGCRenderResources->m_pDirectCmdListAlloc, nullptr);
	if (GC_CHECK_HRESULT(hr, "Failed to Reset command list") == false) {
		return false;
	}
	return true;
}

void GCRenderContext::ExecuteCommandList() 
{
	ID3D12CommandList* cmdsLists[] = { m_pGCRenderResources->m_pCommandList };
	m_pGCRenderResources->m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
}

bool GCRenderContext::CloseCommandList() 
{
	HRESULT hr = m_pGCRenderResources->m_pCommandList->Close();
	if (GC_CHECK_HRESULT(hr, "Failed to Close command list") == false) {
		return false;
	}
	return true;
}

bool GCRenderContext::InitDX12RenderPipeline()
{
	EnableDebugController();

	CreateDXGIFactory1(IID_PPV_ARGS(&m_pGCRenderResources->m_pFactory));



	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_pGCRenderResources->m_pDevice));



	//exit(0);

	// Fallback to WARP device.
	if (FAILED(hardwareResult))
	{
		IDXGIAdapter* pWarpAdapter;
		m_pGCRenderResources->m_pFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));

		D3D12CreateDevice(pWarpAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_pGCRenderResources->m_pDevice));
	}

	m_pGCRenderResources->m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pGCRenderResources->m_pFence));

	m_pGCRenderResources->m_rtvDescriptorSize = m_pGCRenderResources->m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_pGCRenderResources->m_dsvDescriptorSize = m_pGCRenderResources->m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_pGCRenderResources->m_cbvSrvUavDescriptorSize = m_pGCRenderResources->m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = m_pGCRenderResources->m_BackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;

	m_pGCRenderResources->m_pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&msQualityLevels, sizeof(msQualityLevels));

	m_pGCRenderResources->m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m_pGCRenderResources->m_4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	#ifdef _DEBUG
		LogAdapters();
	#endif

	CreateCommandObjects();
	CreateSwapChain();
	
	//Create RTV/DSV Descriptor Heaps
	m_pGCRenderResources->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_pGCRenderResources->m_swapChainBufferCount + 6, false, &m_pGCRenderResources->m_pRtvHeap);
	m_pGCRenderResources->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 2, false, &m_pGCRenderResources->m_pDsvHeap);
	//Create CBV/SRV/UAV Descriptor Heaps
	m_pGCRenderResources->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1000, true, &m_pGCRenderResources->m_pCbvSrvUavDescriptorHeap);

	//*


	m_pGCRenderResources->m_canResize = true;

	OnResize();
	CreateDeferredLightPassResources();
	m_pCbLightPropertiesInstance = new GCUploadBuffer<GCLIGHT>(m_pGCRenderResources->m_pDevice, 100, true);

	// Pixel Id Mapping Output Rtv
	m_pPixelIdMappingBufferRtv = m_pGCRenderResources->CreateRTVTexture(m_pGCRenderResources->GetBackBufferFormat(), D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	return true;
}

void GCRenderContext::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	m_pGCRenderResources->m_pDevice->CreateCommandQueue(
		&queueDesc, 
		IID_PPV_ARGS(&m_pGCRenderResources->m_pCommandQueue)
	);

	m_pGCRenderResources->m_pDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, 
		IID_PPV_ARGS(&m_pGCRenderResources->m_pDirectCmdListAlloc)
	);

	m_pGCRenderResources->m_pDevice->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pGCRenderResources->m_pDirectCmdListAlloc, // Associated command allocator
		nullptr, // Initial PipelineStateObject
		IID_PPV_ARGS(&m_pGCRenderResources->m_pCommandList)
	);

	m_pGCRenderResources->m_pCommandList->Close();
}

void GCRenderContext::CreateSwapChain()
{
	// Release the previous swapchain we will be recreating.
	if (m_pGCRenderResources->m_pSwapChain != nullptr)
	{
		m_pGCRenderResources->m_pSwapChain->Release();
	}

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = m_pGCRenderResources->m_renderWidth;
	sd.BufferDesc.Height = m_pGCRenderResources->m_renderHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_pGCRenderResources->m_BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m_pGCRenderResources->m_4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m_pGCRenderResources->m_4xMsaaState ? (m_pGCRenderResources->m_4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2;
	sd.OutputWindow = m_pGCRenderResources->m_pWindow->GetHMainWnd();
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// #NOTE -> Swap chain uses queue to perform flush.
	m_pGCRenderResources->m_pFactory->CreateSwapChain(m_pGCRenderResources->m_pCommandQueue, &sd, &m_pGCRenderResources->m_pSwapChain);
}

void GCRenderContext::CreatePostProcessingResources(std::string shaderfilePath, std::string csoDestinationPath) {
	{
		D3D12_CLEAR_VALUE* clearValue = new D3D12_CLEAR_VALUE();
		clearValue->Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		clearValue->Color[0] = 0.0f;
		clearValue->Color[1] = 0.0f;
		clearValue->Color[2] = 0.0f;
		clearValue->Color[3] = 1.0f;

		m_pPostProcessingRtv = GetRenderResources()->CreateRTVTexture(m_pGCRenderResources->GetBackBufferFormat(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		// #TODO Ne sert a rien ? Gianni
		//m_postProcessingSrvGpuHandle = GetRenderResources()->CreateSrvWithTexture(m_pPostProcessingRtv->resource, DXGI_FORMAT_R8G8B8A8_UNORM);
		m_postProcessingUavGpuHandle = GetRenderResources()->CreateUavTexture(m_pPostProcessingRtv->pResource);

		// Create a post Processing Shader
		m_postProcessingShaderCS = new GCComputeShader();

		int flags = 0;
		GC_SET_FLAG(flags, GC_VERTEX_POSITION);
		GC_SET_FLAG(flags, GC_VERTEX_UV);

		m_postProcessingShaderCS->Initialize(this, shaderfilePath, csoDestinationPath, flags);
		m_postProcessingShaderCS->Load();
	}
}

void GCRenderContext::CreateDeferredLightPassResources() {
	{
		//GBuffer Creation
		m_pAlbedoGBuffer = m_pGCRenderResources->CreateRTVTexture(m_pGCRenderResources->GetBackBufferFormat(), D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		m_pWorldPosGBuffer = m_pGCRenderResources->CreateRTVTexture(m_pGCRenderResources->m_rgbaFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		m_pNormalGBuffer = m_pGCRenderResources->CreateRTVTexture(m_pGCRenderResources->m_rgbaFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	}

	{
		//Rtv for draw light pass
		m_pDeferredLightPassBufferRtv = m_pGCRenderResources->CreateRTVTexture(m_pGCRenderResources->GetBackBufferFormat(), D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		//Shader light pass
		m_pDeferredLightPassShader = new GCShader();
		std::string shaderFilePath = "../../../res/Shaders/DeferredLightPass.hlsl";
		std::string csoDestinationPath = "../../../res/CsoCompiled/PostProcessing";

		int flags = 0;
		GC_SET_FLAG(flags, GC_VERTEX_POSITION);
		GC_SET_FLAG(flags, GC_VERTEX_UV);

		int rootParametersFlag = 0;
		GC_SET_FLAG(rootParametersFlag, GC_ROOT_PARAMETER_CB0);
		GC_SET_FLAG(rootParametersFlag, GC_ROOT_PARAMETER_CB1);
		GC_SET_FLAG(rootParametersFlag, GC_ROOT_PARAMETER_CB2);
		GC_SET_FLAG(rootParametersFlag, GC_ROOT_PARAMETER_DESCRIPTOR_TABLE_SLOT1);
		GC_SET_FLAG(rootParametersFlag, GC_ROOT_PARAMETER_DESCRIPTOR_TABLE_SLOT2);
		GC_SET_FLAG(rootParametersFlag, GC_ROOT_PARAMETER_DESCRIPTOR_TABLE_SLOT3);
		GC_SET_FLAG(rootParametersFlag, GC_ROOT_PARAMETER_DESCRIPTOR_TABLE_SLOT4);
		//

		m_pDeferredLightPassShader->Initialize(this, shaderFilePath, csoDestinationPath, flags, D3D12_CULL_MODE_BACK, rootParametersFlag);
		m_pDeferredLightPassShader->Load();
	}


	m_pCbMaterialDsl = new GCUploadBuffer<GC_MATERIAL_DSL>(m_pGCRenderResources->m_pDevice, 100, true);
}

void GCRenderContext::OnResize() 
{
	// #TODO NeedLess ?
	
	//m_pGCRenderResources->m_canResize = true;
	//if (m_pGCRenderResources->m_canResize == false)
	//	return;

	GC_CHECK_POINTERSNULL("Device not null", "Device is null", m_pGCRenderResources->m_pDevice);
	GC_CHECK_POINTERSNULL("SwapChain not null", "SwapChain is null", m_pGCRenderResources->m_pDevice);
	GC_CHECK_POINTERSNULL("Command Allocator not null", "Command Allocator is null", m_pGCRenderResources->m_pDevice);

	FlushCommandQueue();
	ResetCommandList();

	ReleasePreviousResources();
	ResizeSwapChain();

	CreateRenderTargetViews();
	CreateDepthStencilBufferAndView();

	CloseCommandList();
	ExecuteCommandList();
	FlushCommandQueue();

	UpdateViewport();
}

void GCRenderContext::ReleasePreviousResources() 
{
	for (int i = 0; i < m_pGCRenderResources->m_swapChainBufferCount; ++i)
	{
		if (m_pGCRenderResources->m_pSwapChainBuffer[i] != nullptr)
		{
			m_pGCRenderResources->m_pSwapChainBuffer[i]->Release();
		}
	}
	if (m_pGCRenderResources->m_pDepthStencilBuffer != nullptr)
	{
		m_pGCRenderResources->m_pDepthStencilBuffer->Release();
	}
}

void GCRenderContext::ResizeSwapChain() 
{
	m_pGCRenderResources->m_pSwapChain->ResizeBuffers(
		m_pGCRenderResources->m_swapChainBufferCount,
		m_pGCRenderResources->m_renderWidth, m_pGCRenderResources->m_renderHeight,
		m_pGCRenderResources->m_BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	);

	m_pGCRenderResources->m_currBackBuffer = 0;
}

void GCRenderContext::CreateRenderTargetViews() 
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_pGCRenderResources->m_pRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < m_pGCRenderResources->m_swapChainBufferCount; i++)
	{
		m_pGCRenderResources->m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pGCRenderResources->m_pSwapChainBuffer[i]));
		m_pGCRenderResources->m_pDevice->CreateRenderTargetView(m_pGCRenderResources->m_pSwapChainBuffer[i], nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, m_pGCRenderResources->m_rtvDescriptorSize);

		// Post processing
		if (i == 0) {
			m_postProcessingFrontBufferSrvGpuHandle = m_pGCRenderResources->CreateStaticSrvWithTexture(m_pGCRenderResources->m_pSwapChainBuffer[i], DXGI_FORMAT_R8G8B8A8_UNORM);
		}
		if (i == 1) {
			m_postProcessingBackBufferSrvGpuHandle = m_pGCRenderResources->CreateStaticSrvWithTexture(m_pGCRenderResources->m_pSwapChainBuffer[i], DXGI_FORMAT_R8G8B8A8_UNORM);
		}
	}

}

void GCRenderContext::CreateDepthStencilBufferAndView()
{
	auto dsv1 = m_pGCRenderResources->CreateDepthStencilBufferAndView(DXGI_FORMAT_D24_UNORM_S8_UINT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_pGCRenderResources->m_pDepthStencilBuffer = dsv1->pResource;
	m_pGCRenderResources->m_depthStencilBufferAddress = dsv1->cpuHandle;

	m_pPixelIdMappingDepthStencilBuffer = m_pGCRenderResources->CreateDepthStencilBufferAndView(DXGI_FORMAT_D24_UNORM_S8_UINT, D3D12_RESOURCE_STATE_DEPTH_WRITE);

}

void GCRenderContext::UpdateViewport() 
{
	m_pGCRenderResources->m_ScreenViewport.TopLeftX = static_cast<FLOAT>(m_pGCRenderResources->m_pWindow->GetClientWidth() - m_pGCRenderResources->m_renderWidth) / 2.0f;
	m_pGCRenderResources->m_ScreenViewport.TopLeftY = static_cast<FLOAT>(m_pGCRenderResources->m_pWindow->GetClientHeight() - m_pGCRenderResources->m_renderHeight) / 2.0f;
	m_pGCRenderResources->m_ScreenViewport.Width = static_cast<float>(m_pGCRenderResources->m_renderWidth);
	m_pGCRenderResources->m_ScreenViewport.Height = static_cast<float>(m_pGCRenderResources->m_renderHeight);
	m_pGCRenderResources->m_ScreenViewport.MinDepth = 0.0f;
	m_pGCRenderResources->m_ScreenViewport.MaxDepth = 1.0f;

	m_pGCRenderResources->m_ScissorRect = {
		(m_pGCRenderResources->m_pWindow->GetClientWidth() - m_pGCRenderResources->m_renderWidth) / 2,
		(m_pGCRenderResources->m_pWindow->GetClientHeight() - m_pGCRenderResources->m_renderHeight) / 2,
		(m_pGCRenderResources->m_pWindow->GetClientWidth() - m_pGCRenderResources->m_renderWidth)/ 2 + m_pGCRenderResources->m_renderWidth,
		(m_pGCRenderResources->m_pWindow->GetClientHeight() - m_pGCRenderResources->m_renderHeight) / 2 + m_pGCRenderResources->m_renderHeight
	};
}

bool GCRenderContext::FlushCommandQueue()
{
	HRESULT hr;
	// Advance the fence value to mark commands up to this fence point.
	m_pGCRenderResources->m_CurrentFence++;
	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	hr = m_pGCRenderResources->m_pCommandQueue->Signal(m_pGCRenderResources->m_pFence, m_pGCRenderResources->m_CurrentFence);
	if (!GC_CHECK_HRESULT(hr, "m_CommandQueue->Signal")) {
		return false;
	};

	// Wait until the GPU has completed commands up to this fence point.
	if (m_pGCRenderResources->m_pFence->GetCompletedValue() < m_pGCRenderResources->m_CurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, NULL, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		hr = m_pGCRenderResources->m_pFence->SetEventOnCompletion(m_pGCRenderResources->m_CurrentFence, eventHandle);
		if (!GC_CHECK_HRESULT(hr, "Fence->SetEventOnCompletion")) {
			return false;
		};

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	return true;
}

bool GCRenderContext::PrepareDraw()
{
	//Always needs to be called right before drawing!!!

	HRESULT hr = m_pGCRenderResources->m_pDirectCmdListAlloc->Reset();
	if (!GC_CHECK_HRESULT(hr, "m_DirectCmdListAlloc->Reset()")) {
		return false;
	};

	hr = m_pGCRenderResources->m_pCommandList->Reset(m_pGCRenderResources->m_pDirectCmdListAlloc, nullptr);

	if (!GC_CHECK_HRESULT(hr, "m_CommandList->Reset()")) {
		return false;
	};

	// Swap
	CD3DX12_RESOURCE_BARRIER ResBar(CD3DX12_RESOURCE_BARRIER::Transition(m_pGCRenderResources->CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &ResBar);

	m_pGCRenderResources->m_pCommandList->RSSetViewports(1, &m_pGCRenderResources->m_ScreenViewport);
	m_pGCRenderResources->m_pCommandList->RSSetScissorRects(1, &m_pGCRenderResources->m_ScissorRect);

	m_pGCRenderResources->m_pCommandList->ClearRenderTargetView(m_pGCRenderResources->CurrentBackBufferViewAddress(), DirectX::Colors::LightBlue, 1, &m_pGCRenderResources->m_ScissorRect);
	m_pGCRenderResources->m_pCommandList->ClearDepthStencilView(m_pGCRenderResources->GetDepthStencilViewAddress(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);


	if (m_isDeferredLightPassActivated) {
		m_pGCRenderResources->m_pCommandList->ClearRenderTargetView(m_pAlbedoGBuffer->cpuHandle, DirectX::Colors::LightBlue, 1, &m_pGCRenderResources->m_ScissorRect);
		m_pGCRenderResources->m_pCommandList->ClearRenderTargetView(m_pWorldPosGBuffer->cpuHandle, DirectX::Colors::LightBlue, 1, &m_pGCRenderResources->m_ScissorRect);
		m_pGCRenderResources->m_pCommandList->ClearRenderTargetView(m_pNormalGBuffer->cpuHandle, DirectX::Colors::LightBlue, 1, &m_pGCRenderResources->m_ScissorRect);
	}
	

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsvs;

	{
		// Basic draw
		D3D12_CPU_DESCRIPTOR_HANDLE basicRtv = m_pGCRenderResources->CurrentBackBufferViewAddress();
		rtvs.push_back(basicRtv);

		if (m_renderMode == GC_RENDER_MODE_3D)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE basicDsv = m_pGCRenderResources->GetDepthStencilViewAddress();
			dsvs.push_back(basicDsv);
		}
	}

	{
		// Pixel Id Mapping
		if (m_isPixelIDMappingActivated) {
			rtvs.push_back(m_pPixelIdMappingBufferRtv->cpuHandle);
		}
	}
	
	{
		// Deferred Light Pass
		if (m_isDeferredLightPassActivated) {
			rtvs.push_back(m_pAlbedoGBuffer->cpuHandle);
			rtvs.push_back(m_pWorldPosGBuffer->cpuHandle);
			rtvs.push_back(m_pNormalGBuffer->cpuHandle);
		}
	}

	UINT rtvCount = static_cast<UINT>(rtvs.size());

	D3D12_CPU_DESCRIPTOR_HANDLE basicDsv = m_pGCRenderResources->GetDepthStencilViewAddress();
	m_pGCRenderResources->m_pCommandList->OMSetRenderTargets(rtvCount, rtvs.data(), FALSE, dsvs.data());

	
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_pGCRenderResources->m_pCbvSrvUavDescriptorHeap };
	m_pGCRenderResources->m_pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	return true;
}

bool GCRenderContext::DrawObject(GCMesh* pMesh, GCMaterial* pMaterial, bool alpha)
{
	GCShader* pShader = pMaterial->GetShader();
	if (pMaterial == nullptr || pShader == nullptr || pMesh == nullptr)
		return false;
	if (!GC_COMPARE_SHADER_MESH_FLAGS(pMaterial, pMesh))
		return false;

	//Basic Draw
	{
		m_pGCRenderResources->m_pCommandList->SetPipelineState(pMaterial->GetShader()->GetPso(alpha));
		m_pGCRenderResources->m_pCommandList->SetGraphicsRootSignature(pMaterial->GetShader()->GetRootSign());

		m_pGCRenderResources->m_pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = pMesh->GetBufferGeometryData()->VertexBufferView();
		m_pGCRenderResources->m_pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		D3D12_INDEX_BUFFER_VIEW indexBufferView = pMesh->GetBufferGeometryData()->IndexBufferView();
		m_pGCRenderResources->m_pCommandList->IASetIndexBuffer(&indexBufferView);

		// Update texture if material has texture
		pMaterial->UpdateTexture();

		int rootParameterFlag = pMaterial->GetShader()->GetFlagRootParameters();

		// Update cb0, cb of object
		if (GC_HAS_FLAG(rootParameterFlag, GC_ROOT_PARAMETER_CB0)) {
			m_pGCRenderResources->m_pCommandList->SetGraphicsRootConstantBufferView(pShader->m_rootParameter_ConstantBuffer_0, pMaterial->GetCbObjectInstance()[pMaterial->GetCount()]->Resource()->GetGPUVirtualAddress());
		}

		// Set cb object buffer on used
		pMaterial->GetCbObjectInstance()[pMaterial->GetCount()]->m_isUsed = true;
		pMaterial->IncrementCBCount();

		// cb1, Camera
		if (GC_HAS_FLAG(rootParameterFlag, GC_ROOT_PARAMETER_CB1)) {
			m_pGCRenderResources->m_pCommandList->SetGraphicsRootConstantBufferView(pShader->m_rootParameter_ConstantBuffer_1, m_pCbCurrentViewProjInstance->Resource()->GetGPUVirtualAddress());
		}
		// cb2, Material Properties
		if (GC_HAS_FLAG(rootParameterFlag, GC_ROOT_PARAMETER_CB2))
			m_pGCRenderResources->m_pCommandList->SetGraphicsRootConstantBufferView(pShader->m_rootParameter_ConstantBuffer_2, pMaterial->GetCbMaterialPropertiesInstance()->Resource()->GetGPUVirtualAddress());
		// cb3, Light Properties
		if (GC_HAS_FLAG(rootParameterFlag, GC_ROOT_PARAMETER_CB3))
			m_pGCRenderResources->m_pCommandList->SetGraphicsRootConstantBufferView(pShader->m_rootParameter_ConstantBuffer_3, m_pCbLightPropertiesInstance->Resource()->GetGPUVirtualAddress());

		// Draw
		m_pGCRenderResources->m_pCommandList->DrawIndexedInstanced(pMesh->GetBufferGeometryData()->IndexCount, 1, 0, 0, 0);
	}

	// Structured Buffer Send to Deferred Shader
	GC_MATERIAL_DSL newMaterial;
	newMaterial.ambientLightColor = pMaterial->ambientLightColor;
	newMaterial.ambient = pMaterial->ambient;
	newMaterial.diffuse = pMaterial->diffuse;
	newMaterial.specular = pMaterial->specular;
	newMaterial.shininess = pMaterial->shininess;
	newMaterial.materialId = pMaterial->m_materialId;

	//Material already in array?
	for (const auto& material : m_materialsUsedInFrame)
	{
		if (material.materialId == newMaterial.materialId)
		{
			
			return true;
		}
	}
	m_materialsUsedInFrame.push_back(newMaterial);

	return true;
}

bool GCRenderContext::CompleteDraw()
{
	if (m_isDeferredLightPassActivated) PerformDeferredLightPass();
	if (m_isCSPostProcessingActivated) PerformPostProcessingCS();
	
	if (m_isCSPostProcessingActivated == false) {
		CD3DX12_RESOURCE_BARRIER RtToPresent = CD3DX12_RESOURCE_BARRIER::Transition(m_pGCRenderResources->CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &RtToPresent);
	}

	HRESULT hr = m_pGCRenderResources->m_pCommandList->Close();
	if (GC_CHECK_HRESULT(hr, "Failed to close command list") == false) return false;
	ID3D12CommandList* cmdsLists[] = { m_pGCRenderResources->m_pCommandList };
	m_pGCRenderResources->m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);


	hr = m_pGCRenderResources->m_pSwapChain->Present(0, 0);
	if (GC_CHECK_HRESULT(hr, "Failed to present swap chain") == false) return false;
	// Swap front - back buffer index
	m_pGCRenderResources->m_currBackBuffer = (m_pGCRenderResources->m_currBackBuffer + 1) % m_pGCRenderResources->m_swapChainBufferCount;


	//Clear frame counter for resource using - Release short time resource
	// #WARNING It will replace resource on these offset already allocated
	m_pGCRenderResources->m_srvStaticOffsetCount = 300;
	m_pGCRenderResources->m_srvDynamicOffsetCount = 320;


	// Flush the command queue
	if (FlushCommandQueue() == false) return false;

	return true;
}

void GCRenderContext::PerformDeferredLightPass() {
	m_pGCRenderResources->m_pCommandList->OMSetRenderTargets(1, &m_pDeferredLightPassBufferRtv->cpuHandle, FALSE, nullptr);

	// Root Sign / Pso
	m_pGCRenderResources->m_pCommandList->SetPipelineState(m_pDeferredLightPassShader->GetPso(true));
	m_pGCRenderResources->m_pCommandList->SetGraphicsRootSignature(m_pDeferredLightPassShader->GetRootSign());

	//deferred light pass entry texture (albedo, worldPosition, normal) linking to shader
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = m_pGCRenderResources->CreateDynamicSrvWithTexture(m_pPixelIdMappingBufferRtv->pResource, m_pGCRenderResources->GetBackBufferFormat());
	m_pGCRenderResources->m_pCommandList->SetGraphicsRootDescriptorTable(m_pDeferredLightPassShader->m_rootParameter_DescriptorTable_1, srvGpuHandle);

	srvGpuHandle = m_pGCRenderResources->CreateDynamicSrvWithTexture(m_pAlbedoGBuffer->pResource, m_pGCRenderResources->GetBackBufferFormat());
	m_pGCRenderResources->m_pCommandList->SetGraphicsRootDescriptorTable(m_pDeferredLightPassShader->m_rootParameter_DescriptorTable_2, srvGpuHandle);

	srvGpuHandle = m_pGCRenderResources->CreateDynamicSrvWithTexture(m_pWorldPosGBuffer->pResource, m_pGCRenderResources->m_rgbaFormat);
	m_pGCRenderResources->m_pCommandList->SetGraphicsRootDescriptorTable(m_pDeferredLightPassShader->m_rootParameter_DescriptorTable_3, srvGpuHandle);

	//srvGpuHandle = m_pGCRenderResources->CreateSrvWithTexture(m_pGCRenderResources->m_DepthStencilBuffer, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
	//m_pGCRenderResources->m_CommandList->SetGraphicsRootDescriptorTable(m_pDeferredLightPassShader->m_rootParameter_DescriptorTable_3, srvGpuHandle);

	srvGpuHandle = m_pGCRenderResources->CreateDynamicSrvWithTexture(m_pNormalGBuffer->pResource, m_pGCRenderResources->m_rgbaFormat);
	m_pGCRenderResources->m_pCommandList->SetGraphicsRootDescriptorTable(m_pDeferredLightPassShader->m_rootParameter_DescriptorTable_4, srvGpuHandle);

	// Draw quad on deferred light pass rtv
	GCMesh* theMesh = m_pGCRenderResources->m_pGraphics->GetMeshes()[0];
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = theMesh->GetBufferGeometryData()->VertexBufferView();
	D3D12_INDEX_BUFFER_VIEW indexBufferView = theMesh->GetBufferGeometryData()->IndexBufferView();
	m_pGCRenderResources->m_pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	m_pGCRenderResources->m_pCommandList->IASetIndexBuffer(&indexBufferView);
	m_pGCRenderResources->m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set camera & lights entry
	m_pGCRenderResources->m_pCommandList->SetGraphicsRootConstantBufferView(m_pDeferredLightPassShader->m_rootParameter_ConstantBuffer_0, m_pCbCurrentViewProjInstance->Resource()->GetGPUVirtualAddress());
	m_pGCRenderResources->m_pCommandList->SetGraphicsRootConstantBufferView(m_pDeferredLightPassShader->m_rootParameter_ConstantBuffer_1, m_pCbLightPropertiesInstance->Resource()->GetGPUVirtualAddress());

	//Update Materials
	size_t count = m_materialsUsedInFrame.size();
	m_pCbMaterialDsl->CopyData(0, m_materialsUsedInFrame.data(), sizeof(GC_MATERIAL_DSL) * count);

	m_pGCRenderResources->m_pCommandList->SetGraphicsRootConstantBufferView(m_pDeferredLightPassShader->m_rootParameter_ConstantBuffer_2, m_pCbMaterialDsl->Resource()->GetGPUVirtualAddress());

	m_pGCRenderResources->m_pCommandList->DrawIndexedInstanced(theMesh->GetBufferGeometryData()->IndexCount, 1, 0, 0, 0);

	CD3DX12_RESOURCE_BARRIER RtToCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(m_pDeferredLightPassBufferRtv->pResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &RtToCopySrc);
	CD3DX12_RESOURCE_BARRIER PixelShaderToCopyDst = CD3DX12_RESOURCE_BARRIER::Transition(m_pGCRenderResources->CurrentBackBuffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &PixelShaderToCopyDst);

	// Copy On Final
	m_pGCRenderResources->m_pCommandList->CopyResource(m_pGCRenderResources->CurrentBackBuffer(), m_pDeferredLightPassBufferRtv->pResource);

	//
	CD3DX12_RESOURCE_BARRIER DstToPresent = CD3DX12_RESOURCE_BARRIER::Transition(m_pGCRenderResources->CurrentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &DstToPresent);
	CD3DX12_RESOURCE_BARRIER SrcToRt = CD3DX12_RESOURCE_BARRIER::Transition(m_pDeferredLightPassBufferRtv->pResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &SrcToRt);
}

void GCRenderContext::PerformPostProcessingCS()
{
	CD3DX12_RESOURCE_BARRIER barrierToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pPostProcessingRtv->pResource,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &barrierToUAV);

	// Transition CurrentBackBuffer to PIXEL_SHADER_RESOURCE for read
	CD3DX12_RESOURCE_BARRIER barrierToShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pGCRenderResources->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &barrierToShaderResource);

	// Set up compute shader
	m_pGCRenderResources->m_pCommandList->SetPipelineState(m_postProcessingShaderCS->GetPso());
	m_pGCRenderResources->m_pCommandList->SetComputeRootSignature(m_postProcessingShaderCS->GetRootSign());

	// Set descriptor tables for compute shader
	int currentBackBuffer = GetRenderResources()->GetCurrBackBuffer();

	if (currentBackBuffer == 0) {
		m_pGCRenderResources->m_pCommandList->SetComputeRootDescriptorTable(0, m_postProcessingFrontBufferSrvGpuHandle);
	}
	if (currentBackBuffer == 1) {
		m_pGCRenderResources->m_pCommandList->SetComputeRootDescriptorTable(0, m_postProcessingBackBufferSrvGpuHandle);
	}



	// UAV descriptor handle for the output texture (PostProcessingRtv)

	// Output texture
	m_pGCRenderResources->m_pCommandList->SetComputeRootDescriptorTable(1, m_postProcessingUavGpuHandle);
	// Dispatch compute shader
	m_pGCRenderResources->m_pCommandList->Dispatch
	(
		(m_pGCRenderResources->GetRenderWidth() + 15) / 16,
		(m_pGCRenderResources->GetRenderHeight() + 15) / 16,
		1
	);

	// Transition PostProcessingRtv to COPY_SOURCE for read
	CD3DX12_RESOURCE_BARRIER barrierToUAV2 = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pPostProcessingRtv->pResource,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	);

	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &barrierToUAV2);

	// Transition CurrentBackBuffer to COPY_DEST for write
	CD3DX12_RESOURCE_BARRIER barrierToCopyFromBackBuffer2 = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pGCRenderResources->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST
	);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &barrierToCopyFromBackBuffer2);

	// Copy m_pPostProcessingRtv to CurrentBackBuffer
	m_pGCRenderResources->m_pCommandList->CopyResource(
		m_pGCRenderResources->CurrentBackBuffer(),
		m_pPostProcessingRtv->pResource
	);

	CD3DX12_RESOURCE_BARRIER ResBar2 = CD3DX12_RESOURCE_BARRIER::Transition(
		GetRenderResources()->CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PRESENT
	);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &ResBar2);

	CD3DX12_RESOURCE_BARRIER ResBar3 = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pPostProcessingRtv->pResource,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_PRESENT
	);
	m_pGCRenderResources->m_pCommandList->ResourceBarrier(1, &ResBar3);
}

void GCRenderContext::EnableDebugController()
{
	#if defined(DEBUG) || defined(_DEBUG) 
		// Enable the D3D12 debug layer.
		{
			D3D12GetDebugInterface(IID_PPV_ARGS(&m_pGCRenderResources->m_pDebugController));
			m_pGCRenderResources->m_pDebugController->EnableDebugLayer();
		}
	#endif
}

void GCRenderContext::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;

	while (m_pGCRenderResources->m_pFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void GCRenderContext::LogAdapterOutputs(IDXGIAdapter* pAdapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (pAdapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, m_pGCRenderResources->m_BackBufferFormat);

		ReleaseCom(output);

		++i;
	}
	m_pGCRenderResources->m_canResize = true;
}

void GCRenderContext::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";
		::OutputDebugString(text.c_str());
	}
}

void GCRenderContext::ActiveCSPostProcessing() {
	m_isCSPostProcessingActivated = true;
	
}

void GCRenderContext::DesactiveCSPostProcessing() {
	m_isCSPostProcessingActivated = false;
}

void GCRenderContext::ActivePixelIDMapping() {
	m_isPixelIDMappingActivated = true;
}

void GCRenderContext::DesactivePixelIDMapping() {
	m_isPixelIDMappingActivated = false;
}

void GCRenderContext::ActiveDeferredLightPass() {
	m_isDeferredLightPassActivated = true;
	m_isPixelIDMappingActivated = true;
}

void GCRenderContext::DesactiveDeferredLightPass() {
	m_isDeferredLightPassActivated = false;
}