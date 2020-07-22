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
	void	Render();		//ï`âÊ
	void	Terminate();	//èIóπ
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
private:
	struct MyVertex {
		float x, y;
		float u, v;
	};
	ComPtr<ID3D12RootSignature>		_root_signature;
	ComPtr<ID3D12PipelineState>		_pipeline_state;
	std::vector<uint8_t>			_g_vertex_shader;
	std::vector<uint8_t>			_g_pixel_shader;
	ComPtr<ID3D12Resource>			_vertex_buffer;
	D3D12_VERTEX_BUFFER_VIEW		_buffer_position;
private:
	std::vector<uint8_t>			loadShader(const std::string& in_filename);
private:
	ComPtr<ID3D12DescriptorHeap>	_descriptor_heap_srv;
	ComPtr<ID3D12DescriptorHeap>	_descriptor_heap_smp;
	ComPtr<ID3D12Resource>			_image;
private:
	ComPtr<ID3D12Resource >			createLoadImage(const std::string& in_filename);
	ComPtr<ID3D12DescriptorHeap>	createSrvHeap(ComPtr<ID3D12Resource > in_image);
	ComPtr<ID3D12DescriptorHeap>	createSmpHeap();
};
