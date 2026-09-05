#define WIN32_LEAN_AND_MEAN
#define COBJMACROS

#include <windows.h>
#include <initguid.h>
#include <dxgi.h>
#include <d3d11.h>
#include <stdio.h>

// 链接所需的 DirectX 相关静态库
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")

// 定义原接口函数的函数指针类型
typedef HRESULT(STDMETHODCALLTYPE* GetDesc1_t)(IDXGIAdapter1* pAdapter, DXGI_ADAPTER_DESC1* pDesc);
typedef HRESULT(STDMETHODCALLTYPE* CheckFeatureSupport_t)(ID3D11Device* pDevice, D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize);

// 保存原函数地址的全局指针
static GetDesc1_t         g_RealGetDesc1 = NULL;
static CheckFeatureSupport_t g_RealCheckFeatureSupport = NULL;

/**
 * Hook 函数：修改显卡设备描述信息
 * 覆盖 IDXGIAdapter1::GetDesc1 (虚函数表索引 10)
 */
HRESULT STDMETHODCALLTYPE Hooked_GetDesc1(IDXGIAdapter1* pAdapter, DXGI_ADAPTER_DESC1* pDesc) {
	// 先调用原始 GetDesc1 获取真实数据
	HRESULT hr = g_RealGetDesc1(pAdapter, pDesc);

	// 如果获取成功，重写显卡属性
	if (SUCCEEDED(hr) && pDesc != NULL) {
		pDesc->VendorId = 0x10DE;                                // 厂商 ID (NVIDIA)
		pDesc->DeviceId = 0x2800;                                // 设备 ID
		pDesc->DedicatedVideoMemory = (SIZE_T)32 * 1024 * 1024 * 1024; // 显存大小 (32GB)
		wcscpy_s(pDesc->Description, 128, L"NVIDIA GeForce RTX 5090"); // 设备名称
		OutputDebugStringW(L"[DXGI_HOOK] GetDesc1 修改成功\n");
	}
	return hr;
}

/**
 * Hook 函数：修改 Direct3D 11 特性支持数据
 * 覆盖 ID3D11Device::CheckFeatureSupport (虚函数表索引 31)
 */
HRESULT STDMETHODCALLTYPE Hooked_CheckFeatureSupport(ID3D11Device* pDevice, D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize) {
	// 调用原始函数获取默认支持状态
	HRESULT hr = g_RealCheckFeatureSupport(pDevice, Feature, pFeatureSupportData, FeatureSupportDataSize);

	// 强行开启 OutputMergerLogicOp 特性
	if (SUCCEEDED(hr) && pFeatureSupportData != NULL) {
		if (Feature == D3D11_FEATURE_D3D11_OPTIONS && FeatureSupportDataSize >= sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS)) {
			D3D11_FEATURE_DATA_D3D11_OPTIONS* options = (D3D11_FEATURE_DATA_D3D11_OPTIONS*)pFeatureSupportData;
			options->OutputMergerLogicOp = TRUE;
			OutputDebugStringW(L"[D3D11_HOOK] CheckFeatureSupport 修改成功\n");
		}
	}
	return hr;
}

/**
 * 虚函数表 (VTable) Hook 安装线程
 */
DWORD WINAPI InstallHookThread(LPVOID lpParam) {
	// 延迟等待主程序初始化完成
	Sleep(500);

	// 1. 创建临时 DXGI 工厂与适配器以获取 IDXGIAdapter1 的虚函数表
	IDXGIFactory1* pFactory1 = NULL;
	if (SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&pFactory1)) && pFactory1) {
		IDXGIAdapter1* pAdapter1 = NULL;
		if (SUCCEEDED(IDXGIFactory1_EnumAdapters1(pFactory1, 0, &pAdapter1)) && pAdapter1) {
			void** vtable1 = *(void***)pAdapter1;
			g_RealGetDesc1 = (GetDesc1_t)vtable1[10]; // 保存原始地址

			// 修改内存保护属性，替换虚函数表项
			DWORD oldProtect;
			if (VirtualProtect(&vtable1[10], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
				vtable1[10] = (void*)Hooked_GetDesc1;
				VirtualProtect(&vtable1[10], sizeof(void*), oldProtect, &oldProtect); // 还原保护属性
			}
			IDXGIAdapter1_Release(pAdapter1);
		}
		IDXGIFactory1_Release(pFactory1);
	}

	// 2. 创建临时 D3D11 设备以获取 ID3D11Device 的虚函数表
	ID3D11Device* pTempDevice = NULL;
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	if (SUCCEEDED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 1, D3D11_SDK_VERSION, &pTempDevice, NULL, NULL)) && pTempDevice) {
		void** vtableDev = *(void***)pTempDevice;
		g_RealCheckFeatureSupport = (CheckFeatureSupport_t)vtableDev[31]; // 保存原始地址

		// 修改内存保护属性，替换虚函数表项
		DWORD oldProtect;
		if (VirtualProtect(&vtableDev[31], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
			vtableDev[31] = (void*)Hooked_CheckFeatureSupport;
			VirtualProtect(&vtableDev[31], sizeof(void*), oldProtect, &oldProtect); // 还原保护属性
		}
		ID3D11Device_Release(pTempDevice);
	}

	return 0;
}

/**
 * DLL 入口函数
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hModule);
		// 创建独立线程执行 Hook 操作，避免阻塞 DllMain
		CreateThread(NULL, 0, InstallHookThread, NULL, 0, NULL);
	}
	return TRUE;
}