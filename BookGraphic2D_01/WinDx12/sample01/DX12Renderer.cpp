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

		// デバイス生成 D3D12は 最低でも D3D_FEATURE_LEVEL_11_0 を要求するようだ.
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

		// コマンドリストの作成.
		hr = _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), nullptr, IID_PPV_ARGS(_command_list.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}
	{
		// コマンドキュー用のフェンスを準備.
		_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		hr = _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_queue_fence.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}
	{	// スワップチェインを生成.
		DXGI_SWAP_CHAIN_DESC desc_swap_chain{};
		desc_swap_chain.BufferCount = BUFFER_SIZE;
		desc_swap_chain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc_swap_chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc_swap_chain.OutputWindow = _window_hwnd;
		desc_swap_chain.SampleDesc.Count = 1;
		desc_swap_chain.Windowed = TRUE;
		desc_swap_chain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc_swap_chain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		hr = _factory->CreateSwapChain(_command_queue.Get(), &desc_swap_chain, (IDXGISwapChain**)_swap_chain.GetAddressOf());
		assert(SUCCEEDED(hr));
	}
	{	// ディスクリプタヒープ(RenderTarget用)の作成.
		D3D12_DESCRIPTOR_HEAP_DESC desc_heap{};
		desc_heap.NumDescriptors = BUFFER_SIZE;
		desc_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

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
void	DX12Renderer::Render() {
	int target_index = _swap_chain->GetCurrentBackBufferIndex();
	{
		D3D12_RESOURCE_BARRIER desc_barrier{};
		desc_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc_barrier.Transition.pResource = _render_target[target_index].Get();
		desc_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		desc_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		_command_list.Get()->ResourceBarrier(1, &desc_barrier);
	}
	{
		float clear_color[4] = { 1.0f,0.0f,0.0f,0.0f };
		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = 0; viewport.TopLeftY = 0;
		viewport.Width = (FLOAT)_window_width;
		viewport.Height = (FLOAT)_window_height;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;
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
		desc_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc_barrier.Transition.pResource = _render_target[target_index].Get();
		desc_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		desc_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

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
}

//-------------------------------------------------------------------------
void	DX12Renderer::resorceRender()
{
}

//-------------------------------------------------------------------------
void	DX12Renderer::resorceTerminate()
{
}
