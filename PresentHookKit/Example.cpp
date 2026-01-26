#include "Engine.h"

bool init = false;
std::atomic<bool> Cleaning(false);
std::atomic<bool> Resizing(false);
std::atomic<int> g_PresentCount(0);

void Cleanup(HMODULE hModule);

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!Cleaning.load())
	{
		if (ImGui::GetCurrentContext()) {
			ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_KEYUP) {
			if (wParam == VK_END) {
				Cleaning.store(true);
				return TRUE;
			}
		}
	}

	return CallWindowProc(PresentHook::oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	Resizing.store(true);
	while (g_PresentCount.load() != 0)
		Sleep(0);


	// Release ImGui render target
	if (PresentHook::pRenderTargetView) {
		PresentHook::pRenderTargetView->Release();
		PresentHook::pRenderTargetView = nullptr;
	}

	// Recreate render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	if (SUCCEEDED(PresentHook::pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer))) {
		PresentHook::pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &PresentHook::pRenderTargetView);
		pBackBuffer->Release();
	}

	Resizing.store(false);

	return PresentHook::oResizeBuffers(PresentHook::pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* SwapChain, UINT SyncInterval, UINT Flags)
{
	if (Cleaning.load())
		return PresentHook::oPresent(SwapChain, SyncInterval, Flags);

	g_PresentCount.fetch_add(1);

	if (!init)
	{
		if (SUCCEEDED(SwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&PresentHook::pDevice)))
		{
			std::cout << "[hkPresent] Device acquired successfully\n";
			HWND hwnd = FindWindow(L"UnrealWindow", nullptr);
			PresentHook::pDevice->GetImmediateContext(&PresentHook::pContext);

			PresentHook::pSwapChain = SwapChain;
			SwapChain->GetDesc(&PresentHook::sd);

			if (!hwnd) hwnd = GetForegroundWindow();

			PresentHook::HookResizeBuffers(*hkResizeBuffers);

			PresentHook::InitImGui();

			// Hook WndProc for input handling
			if (hwnd) {
				PresentHook::oWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
			}

			init = true;
		}
		else
			printf("[INFO] Not DX11 (likely DX12)\n");
	}

	if (!PresentHook::oPresent)
	{
		g_PresentCount.fetch_sub(1);
		return 0;
	}

	if (!ImGui::GetCurrentContext())
	{
		printf("[ERROR] ImGui context not found!\n");
		g_PresentCount.fetch_sub(1);
		return PresentHook::oPresent(SwapChain, SyncInterval, Flags);
	}

	// Start the ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Simple DX11 Hook by PeachMarrow12", nullptr, ImGuiWindowFlags_NoCollapse);
	
	ImGui::Text("Hello, world!");

	ImGui::End();

	if (PresentHook::pRenderTargetView) {
		PresentHook::pContext->OMSetRenderTargets(1, &PresentHook::pRenderTargetView, nullptr);

		D3D11_VIEWPORT vp = {};
		vp.Width = (float)PresentHook::sd.BufferDesc.Width;
		vp.Height = (float)PresentHook::sd.BufferDesc.Height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		PresentHook::pContext->RSSetViewports(1, &vp);
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	g_PresentCount.fetch_sub(1);
	return PresentHook::oPresent(SwapChain, SyncInterval, Flags);
}

DWORD MainThread(HMODULE hModule)
{
	AllocConsole();
	FILE* Dummy;
	freopen_s(&Dummy, "CONOUT$", "w", stdout);

	std::cout << "PresentHookKit Example DLL injected!\n";

	MH_STATUS Status = MH_Initialize();
	if (Status != MH_OK)
	{
		printf("[ERROR] MinHook failed to init: %d", Status);
		Cleaning.store(true);
		Cleanup(hModule);
	}

	if (!PresentHook::HookPresent((LPVOID)hkPresent))
	{
		printf("[ERROR] Failed to initialize engine hooks.\n");
		Cleaning.store(true);
		Cleanup(hModule);
	}
	else
		printf("Engine hooks initialized successfully.\n");

	while (!Cleaning.load())
		Sleep(10);

	Cleanup(hModule);

	return 0;
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		Cleaning.store(true);
		break;
	}

	return TRUE;
}

void Cleanup(HMODULE hModule)
{
	Cleaning.store(true);

	int Attempts = 0;

	while (g_PresentCount.load() > 0 && Attempts < 2000)
	{
		Attempts++;
		Sleep(0);  // wait until all Present calls finish
	}
	PresentHook::CleanUp(hModule);
}