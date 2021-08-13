#include    <Windows.h>
#include    <tchar.h>
#include	"DX12Renderer.h"

#define WINDOW_WIDTH  1024
#define WINDOW_HEIGHT 640
#define CLASS_NAME  "CLASS SAMPLE01"
#define PROC_NAME   "sample01"

LRESULT CALLBACK    WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPTSTR lpCmdLine, int nCmdShow)
{
	HWND            hwnd;
	MSG             msg;
	if (!hPreInst) {
		WNDCLASS    my_prog;
		my_prog.style = CS_HREDRAW | CS_VREDRAW;
		my_prog.lpfnWndProc = WndProc;
		my_prog.cbClsExtra = 0;
		my_prog.cbWndExtra = 0;
		my_prog.hInstance = hInstance;
		my_prog.hIcon = NULL;
		my_prog.hCursor = LoadCursor(NULL, IDC_ARROW);
		my_prog.hbrBackground = NULL;
		my_prog.lpszMenuName = NULL;
		my_prog.lpszClassName = _T(CLASS_NAME);

		if (!RegisterClass(&my_prog)) {
			return FALSE;
		}
	}

	RECT rect = {
		(LONG)0,
		(LONG)0,
		(LONG)(WINDOW_WIDTH),
		(LONG)(WINDOW_HEIGHT)
	};
	AdjustWindowRect(
		&rect,                                    // これ
		WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION,  // 
		FALSE                                     // ���j���[�t���O
	);

	hwnd = CreateWindow(_T(CLASS_NAME),
		_T(PROC_NAME),
		WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,           // �E�B���h�E�̕�
		rect.bottom - rect.top,           // �E�B���h�E�̍���
		NULL,
		NULL,
		hInstance,
		NULL);

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	DX12Renderer* renderer = new DX12Renderer();	//������
	renderer->Initialize(hwnd, WINDOW_WIDTH, WINDOW_HEIGHT);
	do {
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		renderer->Render();	//�`��
	} while (msg.message != WM_QUIT);
	renderer->Terminate(); 
	delete	renderer;	//����
	return (int)(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return(DefWindowProc(hwnd, msg, wParam, lParam));
	}
	return (0L);
}
