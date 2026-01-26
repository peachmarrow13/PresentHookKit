#pragma once

#include "ImGui/imgui.h"
#include "ImGui/misc/cpp/imgui_stdlib.h"
#include "ImGui/backends/imgui_impl_win32.h"
#include "ImGui/backends/imgui_impl_dx11.h"
#include "ImGui/imgui_internal.h"

#include "minhook/include/MinHook.h""

#include <atomic>
#include <d3d11.h>
#include <dxgi.h>
#include <Windows.h>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <fstream>
#include <string>
#include <unordered_map>
#include <numbers>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct PresentHook
{
	static bool HookPresent(LPVOID hkPresent);
	static bool HookResizeBuffers(LPVOID hkResizeBuffers);
	static bool InitImGui();
	typedef HRESULT(__stdcall* tPresent)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
	static tPresent oPresent;
	static LPVOID PresentAddr;
	static IDXGISwapChain* pSwapChain;
	static ID3D11Device* pDevice;
	static ID3D11DeviceContext* pContext;
	static ID3D11RenderTargetView* pRenderTargetView;
	static DXGI_SWAP_CHAIN_DESC sd;
	static WNDPROC oWndProc;
	static HWND hwnd;

	// Function pointer type for IDXGISwapChain::ResizeBuffers
	typedef HRESULT(__stdcall* tResizeBuffers)(
		IDXGISwapChain* pSwapChain,
		UINT BufferCount,
		UINT Width,
		UINT Height,
		DXGI_FORMAT NewFormat,
		UINT SwapChainFlags
		);

	// Original pointer
	static tResizeBuffers oResizeBuffers;

	static void CleanUp(HMODULE hModule);
};