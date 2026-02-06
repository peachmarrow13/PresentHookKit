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

WNDCLASSEX WindowClass = { 0 };

bool DeleteWindow();

bool PresentHook::HookPresent(LPVOID hkPresent)
{
	printf("Hooking Present: %p\n", hkPresent);

	
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = DefWindowProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = GetModuleHandle(NULL);
	WindowClass.hIcon = NULL;
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = NULL;
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = L"Sigma";
	WindowClass.hIconSm = NULL;
	RegisterClassEx(&WindowClass);

	ZeroMemory(&PresentHook::sd, sizeof(DXGI_SWAP_CHAIN_DESC));
	PresentHook::sd.BufferCount = 1;
	PresentHook::sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	PresentHook::sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	PresentHook::sd.OutputWindow = CreateWindow(WindowClass.lpszClassName, L"DX11 Window", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, WindowClass.hInstance, NULL);
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
		HMODULE D3D11Module = GetModuleHandleA("d3d11.dll");

		D3D_FEATURE_LEVEL FeatureLevel;
		const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0 };

		DXGI_RATIONAL RefreshRate;
		RefreshRate.Numerator = 60;
		RefreshRate.Denominator = 1;

		DXGI_MODE_DESC BufferDesc;
		BufferDesc.Width = 100;
		BufferDesc.Height = 100;
		BufferDesc.RefreshRate = RefreshRate;
		BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		DXGI_SAMPLE_DESC SampleDesc;
		SampleDesc.Count = 1;
		SampleDesc.Quality = 0;

		DXGI_SWAP_CHAIN_DESC SwapChainDesc;
		SwapChainDesc.BufferDesc = BufferDesc;
		SwapChainDesc.SampleDesc = SampleDesc;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		SwapChainDesc.BufferCount = 1;
		SwapChainDesc.OutputWindow = PresentHook::sd.OutputWindow;
		SwapChainDesc.Windowed = 1;
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		printf("[INFO] Creating new SwapChain...\n");
		if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &FeatureLevelsRequested, 1, D3D11_SDK_VERSION, &PresentHook::sd, &PresentHook::pSwapChain, &PresentHook::pDevice, &FeatureLevel, &PresentHook::pContext)))
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

			PresentHook::pSwapChain->Release();
			PresentHook::pDevice->Release();
			PresentHook::pContext->Release();

			DeleteWindow();

			return true;
		}
		else
		{
			printf("[ERROR] D3D11CreateDeviceAndSwapChain failed...\n");

			if (PresentHook::pSwapChain) PresentHook::pSwapChain->Release();
			if (PresentHook::pContext) PresentHook::pContext->Release();
			if (PresentHook::pDevice) PresentHook::pDevice->Release();

			if (PresentHook::sd.OutputWindow) DeleteWindow();

			return false;
		}
	}

	return false;
}

bool PresentHook::InitImGui()
{
	DWORD ProcId = GetCurrentProcessId();
	EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
		DWORD wndProcId = 0;
		GetWindowThreadProcessId(hwnd, &wndProcId);
		if (wndProcId == GetCurrentProcessId()) {
			*(HWND*)lParam = hwnd;
			return FALSE; // Stop enumeration
		}
		return TRUE; // Continue enumeration
	}, (LPARAM)&PresentHook::hwnd);

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
	if (!IMGUI_CHECKVERSION())
	{
		std::cout << "[ERROR] ImGui version check failed\n";
		return false;
	}
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

bool DeleteWindow()
{
	DestroyWindow(PresentHook::sd.OutputWindow);
	UnregisterClass(WindowClass.lpszClassName, WindowClass.hInstance);
	if (PresentHook::sd.OutputWindow != 0) {
		return FALSE;
	}
	return TRUE;
}
