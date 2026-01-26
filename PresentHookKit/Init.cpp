#include "Engine.h"

DXGI_SWAP_CHAIN_DESC PresentHook::sd = {};
PresentHook::tPresent PresentHook::oPresent = nullptr;
LPVOID PresentHook::PresentAddr = nullptr;
IDXGISwapChain* PresentHook::pSwapChain = nullptr;
ID3D11Device* PresentHook::pDevice = nullptr;
ID3D11DeviceContext* PresentHook::pContext = nullptr;
ID3D11RenderTargetView* PresentHook::pRenderTargetView = nullptr;
PresentHook::tResizeBuffers PresentHook::oResizeBuffers;
HWND PresentHook::hwnd = nullptr;
WNDPROC PresentHook::oWndProc = nullptr;

bool PresentHook::HookPresent(LPVOID hkPresent)
{
	printf("Hooking Present: %p\n", hkPresent);

	WNDCLASSA wc = { 0 };
	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = GetModuleHandleA(nullptr);
	wc.lpszClassName = "DummyWindowClass";
	RegisterClassA(&wc);

	ZeroMemory(&PresentHook::sd, sizeof(DXGI_SWAP_CHAIN_DESC));
	PresentHook::sd.BufferCount = 1;
	PresentHook::sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	PresentHook::sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	PresentHook::sd.OutputWindow = CreateWindowA("DummyWindowClass", "Dummy Window", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
	PresentHook::sd.SampleDesc.Count = 1;
	PresentHook::sd.SampleDesc.Quality = 0;
	PresentHook::sd.Windowed = TRUE;
	PresentHook::sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	PresentHook::sd.BufferDesc.Width = 1;
	PresentHook::sd.BufferDesc.Height = 1;
	PresentHook::sd.BufferDesc.RefreshRate.Numerator = 60;
	PresentHook::sd.BufferDesc.RefreshRate.Denominator = 1;

	if (!PresentHook::sd.OutputWindow)
	{
		printf("[ERROR] Failed to create dummy window...\n");
		return false;
	}

	D3D_FEATURE_LEVEL FeatureLevel;
	D3D_FEATURE_LEVEL FeatureLevelsRequested = D3D_FEATURE_LEVEL_11_0;

	if (PresentHook::pSwapChain)
	{
		printf("[INFO] Using existing SwapChain...\n");
		void** vTable = *reinterpret_cast<void***>(PresentHook::pSwapChain);

		if (!vTable)
		{
			printf("[ERROR] Failed to get SwapChain vTable...\n");
			return false;
		}

		PresentHook::PresentAddr = (LPVOID)vTable[8];
		PresentHook::oPresent = (PresentHook::tPresent)vTable[8]; // store original

		MH_STATUS Status = MH_CreateHook(PresentHook::PresentAddr, (LPVOID)hkPresent, reinterpret_cast<LPVOID*>(&PresentHook::oPresent));
		if (Status != MH_OK)
		{
			printf("[ERROR] MH_CreateHook failed with status: %d\n", Status);
			return false;
		}

		Status = MH_EnableHook(PresentHook::PresentAddr);
		if (Status != MH_OK)
		{
			printf("[ERROR] MH_EnableHook failed with status: %d\n", Status);
			return false;
		}

		PresentHook::pSwapChain->Release();
		if (PresentHook::pContext) PresentHook::pContext->Release();
		if (PresentHook::pDevice) PresentHook::pDevice->Release();

		return true;
	}
	else
	{
		printf("[INFO] Creating new SwapChain...\n");
		if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			&FeatureLevelsRequested, 1, D3D11_SDK_VERSION,
			&PresentHook::sd, &PresentHook::pSwapChain, &PresentHook::pDevice, &FeatureLevel, &PresentHook::pContext)))
		{
			// void** vTable = *reinterpret_cast<void***>(PresentHook::pSwapChain);
			// Present = vTable[8]
			// ResizeBuffers = vTable[13]

			void** vTable;

			if (PresentHook::pSwapChain)
				vTable = *reinterpret_cast<void***>(PresentHook::pSwapChain);
			else
			{
				printf("[ERROR] pSwapChain is null after creation...\n");
				return false;
			}

			if (!vTable)
			{
				printf("[ERROR] Failed to get SwapChain vTable...\n");
				return false;
			}

			PresentHook::PresentAddr = (LPVOID)vTable[8];
			PresentHook::oPresent = (PresentHook::tPresent)vTable[8]; // store original

			MH_STATUS Status = MH_CreateHook(PresentHook::PresentAddr, (LPVOID)hkPresent, reinterpret_cast<LPVOID*>(&PresentHook::oPresent));
			if (Status == MH_ERROR_ALREADY_CREATED)
			{
				printf("[INFO] Present hook already created, removing...\n");
				MH_DisableHook(PresentHook::PresentAddr);
				MH_RemoveHook(PresentHook::PresentAddr);
				Status = MH_CreateHook(PresentHook::PresentAddr, (LPVOID)hkPresent, reinterpret_cast<LPVOID*>(&PresentHook::oPresent));
			}
			if (Status != MH_OK)
			{
				printf("[ERROR] MH_CreateHook failed with status: %d\n", Status);
				return false;
			}

			Status = MH_EnableHook(PresentHook::PresentAddr);
			if (Status != MH_OK)
			{
				printf("[ERROR] MH_EnableHook failed with status: %d\n", Status);
				return false;
			}

			//DWORD oldProtect;
			//VirtualProtect(&vTable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
			//vTable[8] = (void*)&hkPresent; // replace with my hook
			//VirtualProtect(&vTable[8], sizeof(void*), oldProtect, &oldProtect);

			DestroyWindow(PresentHook::sd.OutputWindow);

			PresentHook::pSwapChain->Release();
			PresentHook::pContext->Release();
			PresentHook::pDevice->Release();

			return true;
		}
		else
		{
			printf("[ERROR] D3D11CreateDeviceAndSwapChain failed...\n");
			if (PresentHook::sd.OutputWindow) DestroyWindow(PresentHook::sd.OutputWindow);

			if (PresentHook::pSwapChain) PresentHook::pSwapChain->Release();
			if (PresentHook::pContext) PresentHook::pContext->Release();
			if (PresentHook::pDevice) PresentHook::pDevice->Release();

			return false;
		}
	}

	return false;
}

bool PresentHook::InitImGui()
{
	PresentHook::hwnd = FindWindow(L"UnrealWindow", nullptr); // ONLY UNREAL ENGINE GAMES
	if (!PresentHook::pDevice || !PresentHook::pContext) {
		std::cout << "[ERROR] Device or Context is null...\n";
		Sleep(30);
		return false;
	}

	if (!PresentHook::hwnd) {
		std::cout << "[ERROR] HWND is null...\n";
		Sleep(30);
		return false;
	}

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	if (!ImGui::GetCurrentContext())
	{
		std::cout << "[ERROR] ImGui::CreateContext failed\n";
		return false;
	}

	// Setup Platform/Renderer backends
	if (!ImGui_ImplWin32_Init(PresentHook::hwnd)) {
		std::cout << "[ERROR] ImGui_ImplWin32_Init failed\n";
		return false;
	}
	if (!ImGui_ImplDX11_Init(PresentHook::pDevice, PresentHook::pContext)) {
		std::cout << "[ERROR] ImGui_ImplDX11_Init failed\n";
		ImGui_ImplWin32_Shutdown();
		return false;
	}

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Controller Controls
	io.Fonts->AddFontDefault();
	io.MouseDrawCursor = true;  // Let ImGui draw the cursor

	ImGui::StyleColorsDark();

	if (PresentHook::pSwapChain) { // Create render target if we have a valid swapchain
		ID3D11Texture2D* pBackBuffer = nullptr;
		HRESULT hr = PresentHook::pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
		if (SUCCEEDED(hr)) {
			hr = PresentHook::pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &PresentHook::pRenderTargetView);
			if (SUCCEEDED(hr)) {
				std::cout << "[InitImGui] Render target view created successfully\n";
			}
			else {
				std::cout << "[ERROR] Failed to create render target view: " << std::hex << hr << std::endl;
			}
			pBackBuffer->Release();
		}
		else {
			std::cout << "[ERROR] Failed to get back buffer: " << std::hex << hr << std::endl;
		}
	}

	std::cout << "[InitImGui] ImGui initialized successfully\n";
	return true;
}

// Attach Hook
bool PresentHook::HookResizeBuffers(LPVOID hkResizeBuffers)
{
	//if (!PresentHook::pSwapChain) return false;

	void** vTable = *reinterpret_cast<void***>(PresentHook::pSwapChain);
	PresentHook::oResizeBuffers = (PresentHook::tResizeBuffers)vTable[13]; // store original

	DWORD oldProtect;
	VirtualProtect(&vTable[13], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
	vTable[13] = (void*)hkResizeBuffers; // replace with my hook
	VirtualProtect(&vTable[13], sizeof(void*), oldProtect, &oldProtect);
	return true;
}

void PresentHook::CleanUp(HMODULE hModule)
{
	std::cout << "Cleaning up...\n";

	MH_DisableHook(PresentHook::PresentAddr);
	MH_RemoveHook(PresentHook::PresentAddr);

	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}


	if (PresentHook::hwnd && PresentHook::oWndProc)
	{
		SetWindowLongPtr(PresentHook::hwnd, GWLP_WNDPROC, (LONG_PTR)PresentHook::oWndProc);
		PresentHook::oWndProc = nullptr;
	}

	MH_Uninitialize();

	// Clean up DirectX resources
	if (PresentHook::pRenderTargetView) {
		printf("Releasing render target view...\n");
		PresentHook::pRenderTargetView->Release();
		PresentHook::pRenderTargetView = nullptr;
	}
	if (PresentHook::pContext) {
		printf("Releasing device context...\n");
		PresentHook::pContext->Release();
		PresentHook::pContext = nullptr;
	}
	if (PresentHook::pDevice) {
		printf("Releasing device...\n");
		PresentHook::pDevice->Release();
		PresentHook::pDevice = nullptr;
	}
	if (PresentHook::pSwapChain) {
		printf("Releasing swap chain...\n");
		PresentHook::pSwapChain->Release();
		PresentHook::pSwapChain = nullptr;
	}

	std::cout << "Cleanup complete. Unloading DLL...\n";

	// Clean up console
	FreeConsole();
	FreeLibraryAndExitThread(hModule, 0);
}