#pragma once

#include	<Windows.h>
#include    <cstdint>
#include	<d3d12.h>
#include	<dxgi1_4.h>
#include	<d3dcompiler.h>
#include	<DirectXMath.h>
#include	<wrl/client.h>
#include	<string>
#include	<vector>

using namespace Microsoft::WRL;

class DX12Renderer {
private:
	static const int BUFFER_SIZE = 2;
public:
	DX12Renderer();
public:
	void 	Initialize(HWND in_hwnd, int32_t in_widith, int32_t in_height);
	void	Render();		//ここで編集できてしますので
	void	Terminate();	//�I��
private:
	HWND	_window_hwnd;
	int		_window_width;
	int		_window_height;

	HANDLE	_fence_event;
	ComPtr<ID3D12Device>				_device;
	ComPtr<ID3D12CommandQueue>			_command_queue;
	ComPtr<ID3D12CommandAllocator>		_command_allocator;
	ComPtr<ID3D12GraphicsCommandList>	_command_list;
	ComPtr<IDXGISwapChain3>				_swap_chain;
	ComPtr<ID3D12Fence>					_queue_fence;
	ComPtr<IDXGIFactory3>				_factory;
	ComPtr<ID3D12DescriptorHeap>		_descriptor_heap;
	ComPtr<ID3D12Resource>				_render_target[BUFFER_SIZE];
	D3D12_CPU_DESCRIPTOR_HANDLE			_rtv_handle[BUFFER_SIZE];
private:
	void	resorceInitialize();
	void	resorceRender();
	void	resorceTerminate();
};
