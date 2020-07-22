#include	<fstream>
#include	"DX12Renderer.h"

#include	<fstream>
#include	<gdiplus.h>
#include	<codecvt> 
#include	<cstdio>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment (lib, "gdiplus.lib") 

//-------------------------------------------------------------------------
DX12Renderer::DX12Renderer() 
{
}

//-------------------------------------------------------------------------
void	DX12Renderer::Initialize(HWND in_hwnd, int32_t in_widith, int32_t in_height) 
{
	_window_hwnd = in_hwnd;
	_window_width = in_widith;
	_window_height = in_height;

	UINT flags_DXGI = 0;
	ID3D12Debug* debug = nullptr;
	HRESULT hr;
	{
#if _DEBUG
		D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
		if (debug)
		{
			debug->EnableDebugLayer();
			debug->Release();
		}
		flags_DXGI |= DXGI_CREATE_FACTORY_DEBUG;
#endif
		hr = CreateDXGIFactory2(flags_DXGI, IID_PPV_ARGS(_factory.ReleaseAndGetAddressOf()));
		assert(SUCCEEDED(hr));

		ComPtr<IDXGIAdapter> adapter;
		hr = _factory->EnumAdapters(0, adapter.GetAddressOf());
		assert(SUCCEEDED(hr));

		// デバイス生成.
		// D3D12は 最低でも D3D_FEATURE_LEVEL_11_0 を要求するようだ.
		hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(_device.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}
	{
		// コマンドアロケータを生成.
		hr = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_command_allocator.GetAddressOf()));
		assert(SUCCEEDED(hr));

		// コマンドキューを生成.
		D3D12_COMMAND_QUEUE_DESC desc_command_queue{};
		desc_command_queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc_command_queue.Priority = 0;
		desc_command_queue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		hr = _device->CreateCommandQueue(&desc_command_queue, IID_PPV_ARGS(_command_queue.GetAddressOf()));
		assert(SUCCEEDED(hr));

		// コマンドキュー用のフェンスを準備.
		_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		hr = _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_queue_fence.GetAddressOf()));
		assert(SUCCEEDED(hr));

		// コマンドリストの作成.
		hr = _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), nullptr, IID_PPV_ARGS(_command_list.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}
	{
		// スワップチェインを生成.
		DXGI_SWAP_CHAIN_DESC desc_swap_chain{};
		desc_swap_chain.BufferCount			= BUFFER_SIZE;
		desc_swap_chain.BufferDesc.Format	= DXGI_FORMAT_R8G8B8A8_UNORM;
		desc_swap_chain.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc_swap_chain.OutputWindow		= _window_hwnd;
		desc_swap_chain.SampleDesc.Count	= 1;
		desc_swap_chain.Windowed			= TRUE;
		desc_swap_chain.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc_swap_chain.Flags				= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		hr = _factory->CreateSwapChain(_command_queue.Get(), &desc_swap_chain, (IDXGISwapChain**)_swap_chain.GetAddressOf());
		assert(SUCCEEDED(hr));
	}
	{	// ディスクリプタヒープ(RenderTarget用)の作成.
		D3D12_DESCRIPTOR_HEAP_DESC desc_heap{};
		desc_heap.NumDescriptors	= BUFFER_SIZE;
		desc_heap.Type				= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc_heap.Flags				= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		hr = _device->CreateDescriptorHeap(&desc_heap, IID_PPV_ARGS(_descriptor_heap.GetAddressOf()));
		assert(SUCCEEDED(hr));

		// レンダーターゲット(プライマリ用)の作成.
		UINT stride_handle_bytes = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (UINT i = 0; i < BUFFER_SIZE; ++i) {
			hr = _swap_chain->GetBuffer(i, IID_PPV_ARGS(_render_target[i].GetAddressOf()));
			assert(SUCCEEDED(hr));

			_rtv_handle[i] = _descriptor_heap->GetCPUDescriptorHandleForHeapStart();
			_rtv_handle[i].ptr += i * stride_handle_bytes;
			_device->CreateRenderTargetView(_render_target[i].Get(), nullptr, _rtv_handle[i]);
		}
	}
	resorceInitialize();
}

//-------------------------------------------------------------------------
void	DX12Renderer::Render() 
{
	int target_index = _swap_chain->GetCurrentBackBufferIndex();
	{
		D3D12_RESOURCE_BARRIER desc_barrier{};
		desc_barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc_barrier.Transition.pResource	= _render_target[target_index].Get();
		desc_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		desc_barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_RENDER_TARGET;

		_command_list.Get()->ResourceBarrier(1, &desc_barrier);
	}
	{
		float clear_color[4] = { 1.0f,0.0f,0.0f,0.0f };
		D3D12_VIEWPORT viewport;
		viewport.TopLeftX	= 0;
		viewport.TopLeftY	= 0;
		viewport.Width		= (FLOAT)_window_width;
		viewport.Height		= (FLOAT)_window_height;
		viewport.MinDepth	= 0;
		viewport.MaxDepth	= 1;
		// レンダーターゲットのクリア処理.
		_command_list->RSSetViewports(1, &viewport);
		_command_list->ClearRenderTargetView(_rtv_handle[target_index], clear_color, 0, nullptr);
	}
	//----------------------------------------------------------------------------------
	{
		D3D12_RECT rect = { 0, 0, _window_width, _window_height };
		_command_list->RSSetScissorRects(1, &rect);
		_command_list->OMSetRenderTargets(1, &_rtv_handle[target_index], TRUE, nullptr);
	}
	resorceRender();
	{
		D3D12_RESOURCE_BARRIER desc_barrier{};
		desc_barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc_barrier.Transition.pResource	= _render_target[target_index].Get();
		desc_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		desc_barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_PRESENT;

		_command_list.Get()->ResourceBarrier(1, &desc_barrier);
	}
	{
		_command_list->Close();

		// 積んだコマンドの実行.
		ID3D12CommandList* command_list = _command_list.Get();
		_command_queue->ExecuteCommandLists(1, &command_list);
		_swap_chain->Present(1, 0);
	}
	{
		static UINT64 frames = 0;

		_queue_fence->SetEventOnCompletion(frames, _fence_event);
		_command_queue->Signal(_queue_fence.Get(), frames);
		WaitForSingleObject(_fence_event, INFINITE);
		frames++;
	}
	_command_allocator->Reset();
	_command_list->Reset(_command_allocator.Get(), nullptr);
}
//-------------------------------------------------------------------------
void	DX12Renderer::Terminate() 
{
	resorceTerminate();
	CloseHandle(_fence_event);
}

//-------------------------------------------------------------------------
void	DX12Renderer::resorceInitialize()
{
	HRESULT hr;

	{	//シェーダー読み込み
		_g_vertex_shader	= loadShader("VertexShader.cso");
		_g_pixel_shader		= loadShader("PixelShader.cso");
	}
	{	//RootSignature
		D3D12_ROOT_PARAMETER root_parameters[1]{};	//	固定バッファがあるよ＜新規＞
		root_parameters[0].ParameterType			= D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[0].ShaderVisibility			= D3D12_SHADER_VISIBILITY_ALL;
		root_parameters[0].Descriptor.ShaderRegister = 0;
		root_parameters[0].Descriptor.RegisterSpace = 0;

		D3D12_ROOT_SIGNATURE_DESC desc_root_signature{};
		desc_root_signature.Flags			= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		desc_root_signature.NumParameters	= _countof(root_parameters);;
		desc_root_signature.pParameters		= root_parameters;

		ComPtr<ID3DBlob> root_sig_blob, error_blob;
		hr = D3D12SerializeRootSignature(&desc_root_signature, D3D_ROOT_SIGNATURE_VERSION_1, root_sig_blob.GetAddressOf(), error_blob.GetAddressOf());
		assert(SUCCEEDED(hr));

		hr = _device->CreateRootSignature(0, root_sig_blob->GetBufferPointer(), root_sig_blob->GetBufferSize(), IID_PPV_ARGS(_root_signature.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}
	{	//Piplineの作成
		// 今回のための頂点レイアウト.
		D3D12_INPUT_ELEMENT_DESC desc_input_elements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		// PipelineStateオブジェクトの作成.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_pipeline_state{};
		desc_pipeline_state.VS.pShaderBytecode			= _g_vertex_shader.data();
		desc_pipeline_state.VS.BytecodeLength			= _g_vertex_shader.size();
		desc_pipeline_state.PS.pShaderBytecode			= _g_pixel_shader.data();
		desc_pipeline_state.PS.BytecodeLength			= _g_pixel_shader.size();
		desc_pipeline_state.SampleDesc.Count			= 1;
		desc_pipeline_state.SampleMask					= UINT_MAX;
		desc_pipeline_state.pRootSignature				= _root_signature.Get();
		desc_pipeline_state.NumRenderTargets			= 1;
		desc_pipeline_state.RTVFormats[0]				= DXGI_FORMAT_R8G8B8A8_UNORM;
		desc_pipeline_state.PrimitiveTopologyType		= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc_pipeline_state.InputLayout.pInputElementDescs = desc_input_elements;
		desc_pipeline_state.InputLayout.NumElements		= _countof(desc_input_elements);
		desc_pipeline_state.RasterizerState.CullMode	= D3D12_CULL_MODE_NONE;
		desc_pipeline_state.RasterizerState.FillMode	= D3D12_FILL_MODE_SOLID;
		desc_pipeline_state.RasterizerState.DepthClipEnable = TRUE;
		desc_pipeline_state.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_state;
		for (int i = 0; i < _countof(desc_state.BlendState.RenderTarget); ++i) {
			desc_pipeline_state.BlendState.RenderTarget[i].BlendEnable		= TRUE;
			desc_pipeline_state.BlendState.RenderTarget[i].SrcBlend			= D3D12_BLEND_SRC_ALPHA;
			desc_pipeline_state.BlendState.RenderTarget[i].DestBlend		= D3D12_BLEND_INV_SRC_ALPHA;
			desc_pipeline_state.BlendState.RenderTarget[i].BlendOp			= D3D12_BLEND_OP_ADD;
			desc_pipeline_state.BlendState.RenderTarget[i].SrcBlendAlpha	= D3D12_BLEND_ONE;
			desc_pipeline_state.BlendState.RenderTarget[i].DestBlendAlpha	= D3D12_BLEND_ONE;
			desc_pipeline_state.BlendState.RenderTarget[i].BlendOpAlpha		= D3D12_BLEND_OP_ADD;
			desc_pipeline_state.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
		desc_pipeline_state.DepthStencilState.DepthEnable = FALSE;

		hr = _device->CreateGraphicsPipelineState(&desc_pipeline_state, IID_PPV_ARGS(_pipeline_state.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}
	{	//	モデルの登録
		MyVertex vertices_array[] =
		{
			{ -1.0f,+0.5f, 0.0f },
			{ -0.6f,-0.5f, 0.0f },
			{ -0.2f,+0.5f, 0.0f },
			{ +0.2f,-0.5f, 0.0f },
			{ +0.6f,+0.5f, 0.0f },
			{ +1.0f,-0.5f, 0.0f },
		};

		// 頂点データの作成.
		D3D12_HEAP_PROPERTIES heap_props{};
		heap_props.Type					= D3D12_HEAP_TYPE_UPLOAD;
		heap_props.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heap_props.CreationNodeMask		= 0;
		heap_props.VisibleNodeMask		= 0;

		D3D12_RESOURCE_DESC   desc_resource{};
		desc_resource.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
		desc_resource.Width				= sizeof(vertices_array);
		desc_resource.Height			= 1;
		desc_resource.DepthOrArraySize	= 1;
		desc_resource.MipLevels			= 1;
		desc_resource.Format			= DXGI_FORMAT_UNKNOWN;
		desc_resource.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc_resource.SampleDesc.Count	= 1;

		hr = _device->CreateCommittedResource(&heap_props,
			D3D12_HEAP_FLAG_NONE,
			&desc_resource,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(_vertex_buffer.GetAddressOf()));
		assert(SUCCEEDED(hr));

		// 頂点データの書き込み
		void* mapped = nullptr;
		hr = _vertex_buffer->Map(0, nullptr, &mapped);
		assert(SUCCEEDED(hr));

		memcpy(mapped, vertices_array, sizeof(vertices_array));
		_vertex_buffer->Unmap(0, nullptr);

		_buffer_position.BufferLocation	= _vertex_buffer->GetGPUVirtualAddress();
		_buffer_position.StrideInBytes	= sizeof(MyVertex);
		_buffer_position.SizeInBytes	= sizeof(vertices_array);
	}
}

//-------------------------------------------------------------------------
void	DX12Renderer::resorceRender()
{
	//	パイプライン設定
	_command_list->SetPipelineState(_pipeline_state.Get());

	// シグネイチャー設定
	_command_list->SetGraphicsRootSignature(_root_signature.Get());

	//	プリミティブ登録
#if PRIMITIVE_POINT_LIST
	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
#endif
#if PRIMITIVE_LINE_LIST
	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
#endif
#if	PRIMITIVE_LINE_STRIP
	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
#endif
#if PRIMITIVE_TRIANGLE_LIST
	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
#endif
#if	PRIMITIVE_TRIANGLE_STRIP
	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
#endif
	_command_list->IASetVertexBuffers(0, 1, &_buffer_position);
	_command_list->DrawInstanced(6, 1, 0, 0);
}

//-------------------------------------------------------------------------
void	DX12Renderer::resorceTerminate()
{
	_g_vertex_shader.clear();
	_g_pixel_shader.clear();
}

//-------------------------------------------------------------------------
std::vector<uint8_t>		DX12Renderer::loadShader(const std::string& in_filename)
{
	std::vector<uint8_t>	ret_shader;
#if _DEBUG
	std::string filename = u8"x64/Debug/" + in_filename;
#else
	std::string filename = u8"x64/Release/" + in_filename;
#endif
	// コンパイル済みシェーダーの読み込み.
	FILE* fp = nullptr;
	fopen_s(&fp, filename.c_str(), "rb");
	assert(fp);

	fseek(fp, 0, SEEK_END);
	ret_shader.resize(ftell(fp));
	rewind(fp);
	fread(ret_shader.data(), 1, ret_shader.size(), fp);
	fclose(fp);

	return	ret_shader;
}
