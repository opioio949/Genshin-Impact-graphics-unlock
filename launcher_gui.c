#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>

#define ID_BTN_LAUNCH 2001 // 启动按钮控件 ID

/**
 * 辅助函数：拼接路径并检查目标可执行文件是否存在
 */
static BOOL CheckExeExists(const wchar_t* dir, const wchar_t* sub, wchar_t* outPath, DWORD maxPath) {
	if (!dir || wcslen(dir) == 0) return FALSE;
	swprintf_s(outPath, maxPath, L"%s%s%s", dir, (dir[wcslen(dir) - 1] == L'\\' ? L"" : L"\\"), sub);
	return GetFileAttributesW(outPath) != INVALID_FILE_ATTRIBUTES;
}

/**
 * 辅助函数：读取指定注册表项并验证游戏程序路径
 */
static BOOL SearchRegistryKey(HKEY hRoot, const wchar_t* subKey, const wchar_t* valName, wchar_t* outPath, DWORD maxPath) {
	HKEY hKey;
	if (RegOpenKeyExW(hRoot, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		wchar_t installDir[MAX_PATH] = { 0 };
		DWORD bufSize = sizeof(installDir);
		if (RegQueryValueExW(hKey, valName, NULL, NULL, (LPBYTE)installDir, &bufSize) == ERROR_SUCCESS) {
			RegCloseKey(hKey);
			// 验证常见的子路径格式
			if (CheckExeExists(installDir, L"YuanShen.exe", outPath, maxPath)) return TRUE;
			if (CheckExeExists(installDir, L"Genshin Impact Game\\YuanShen.exe", outPath, maxPath)) return TRUE;
			if (CheckExeExists(installDir, L"games\\Genshin Impact Game\\YuanShen.exe", outPath, maxPath)) return TRUE;
		}
		else {
			RegCloseKey(hKey);
		}
	}
	return FALSE;
}

/**
 * 多途径检索游戏安装路径
 */
BOOL FindYuanShenPath(wchar_t* outPath, DWORD maxPath) {
	// 1. 检索注册表（包含 HKLM 与 HKCU 根键，兼容新旧版本启动器）
	HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
	const wchar_t* regKeys[] = {
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\原神",
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Genshin Impact",
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\HYP_1_1_cn",
		L"SOFTWARE\\miHoYo\\Genshin Impact",
		L"SOFTWARE\\miHoYo\\原神",
		L"SOFTWARE\\miHoYo\\HYP",
		L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\原神",
		L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Genshin Impact",
		L"SOFTWARE\\WOW6432Node\\miHoYo\\Genshin Impact",
		L"SOFTWARE\\WOW6432Node\\miHoYo\\原神"
	};

	for (int r = 0; r < 2; r++) {
		for (size_t i = 0; i < sizeof(regKeys) / sizeof(regKeys[0]); i++) {
			if (SearchRegistryKey(roots[r], regKeys[i], L"InstallPath", outPath, maxPath)) return TRUE;
			if (SearchRegistryKey(roots[r], regKeys[i], L"GamePath", outPath, maxPath)) return TRUE;
			if (SearchRegistryKey(roots[r], regKeys[i], L"Path", outPath, maxPath)) return TRUE;
		}
	}

	// 2. 检查启动器自身的当前目录及其上级目录
	wchar_t selfPath[MAX_PATH] = { 0 };
	if (GetModuleFileNameW(NULL, selfPath, MAX_PATH)) {
		wchar_t* p = wcsrchr(selfPath, L'\\');
		if (p) {
			*p = L'\0'; // 截取为同级目录
			if (CheckExeExists(selfPath, L"YuanShen.exe", outPath, maxPath)) return TRUE;
			if (CheckExeExists(selfPath, L"Genshin Impact Game\\YuanShen.exe", outPath, maxPath)) return TRUE;

			p = wcsrchr(selfPath, L'\\');
			if (p) {
				*p = L'\0'; // 截取为上级目录
				if (CheckExeExists(selfPath, L"YuanShen.exe", outPath, maxPath)) return TRUE;
				if (CheckExeExists(selfPath, L"Genshin Impact Game\\YuanShen.exe", outPath, maxPath)) return TRUE;
			}
		}
	}

	// 3. 扫描常见盘符预设路径
	const wchar_t* commonPaths[] = {
		L".\\YuanShen.exe",
		L".\\Genshin Impact Game\\YuanShen.exe",
		L"C:\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"D:\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"E:\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"F:\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"C:\\Program Files\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"D:\\Program Files\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"D:\\Games\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"E:\\Games\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe",
		L"F:\\Games\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe"
	};

	for (size_t i = 0; i < sizeof(commonPaths) / sizeof(commonPaths[0]); i++) {
		if (GetFileAttributesW(commonPaths[i]) != INVALID_FILE_ATTRIBUTES) {
			GetFullPathNameW(commonPaths[i], maxPath, outPath, NULL);
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * 从程序资源中提取内嵌 DLL 到系统临时目录
 */
BOOL ExtractEmbeddedDll(wchar_t* outDllPath, DWORD maxPath) {
	wchar_t tempDir[MAX_PATH];
	if (!GetTempPathW(MAX_PATH, tempDir)) return FALSE;

	swprintf_s(outDllPath, maxPath, L"%sdxgi_hook_runtime.dll", tempDir);

	// 定位二进制资源 ID 101
	HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(101), (LPCWSTR)RT_RCDATA);
	if (!hRes) return FALSE;

	HGLOBAL hMem = LoadResource(NULL, hRes);
	if (!hMem) return FALSE;

	DWORD size = SizeofResource(NULL, hRes);
	LPVOID pData = LockResource(hMem);
	if (!pData || size == 0) return FALSE;

	// 写入文件到 Temp 目录
	HANDLE hFile = CreateFileW(outDllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;

	DWORD written = 0;
	BOOL result = WriteFile(hFile, pData, size, &written, NULL);
	CloseHandle(hFile);

	return result && (written == size);
}

/**
 * 以挂起状态启动目标进程，并通过远程线程注入指定 DLL
 */
BOOL LaunchAndInject(const wchar_t* exePath, const wchar_t* dllPath) {
	STARTUPINFOW si = { sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION pi = { 0 };

	// 获取游戏主程序所在的工作目录
	wchar_t workingDir[MAX_PATH];
	wcscpy_s(workingDir, MAX_PATH, exePath);
	wchar_t* lastSlash = wcsrchr(workingDir, L'\\');
	if (lastSlash) *lastSlash = L'\0';

	// 1. 以 CREATE_SUSPENDED (挂起状态) 创建游戏进程
	BOOL bSuccess = CreateProcessW(
		exePath, NULL, NULL, NULL, FALSE,
		CREATE_SUSPENDED | DETACHED_PROCESS,
		NULL, workingDir, &si, &pi
	);

	if (!bSuccess) return FALSE;

	// 2. 在目标进程中分配内存并写入 DLL 路径
	size_t pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
	LPVOID pRemoteBuffer = VirtualAllocEx(pi.hProcess, NULL, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!pRemoteBuffer) {
		TerminateProcess(pi.hProcess, 1);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return FALSE;
	}

	WriteProcessMemory(pi.hProcess, pRemoteBuffer, dllPath, pathSize, NULL);

	// 3. 创建远程线程调用 LoadLibraryW 执行注入
	FARPROC pLoadLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
	HANDLE hRemoteThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteBuffer, 0, NULL);

	if (hRemoteThread) {
		WaitForSingleObject(hRemoteThread, 3000); // 等待注入完成
		CloseHandle(hRemoteThread);
	}

	// 4. 恢复游戏主线程，继续运行程序
	ResumeThread(pi.hThread);

	// 清理资源句柄
	VirtualFreeEx(pi.hProcess, pRemoteBuffer, 0, MEM_RELEASE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return TRUE;
}

/**
 * 执行完整的提取、定位与启动流程
 */
void ExecuteLaunchSequence(HWND hwnd) {
	wchar_t exePath[MAX_PATH] = { 0 };
	wchar_t dllPath[MAX_PATH] = { 0 };

	// 释放组件
	if (!ExtractEmbeddedDll(dllPath, MAX_PATH)) {
		MessageBoxW(hwnd, L"解压组件失败", L"错误", MB_ICONERROR);
		return;
	}

	// 定位路径，自动定位失败时触发标准文件选择对话框
	if (!FindYuanShenPath(exePath, MAX_PATH)) {
		if (MessageBoxW(hwnd, L"未找到 YuanShen.exe，是否手动选择路径？", L"提示", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			OPENFILENAMEW ofn = { sizeof(ofn) };
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = L"可执行文件 (YuanShen.exe)\0YuanShen.exe\0所有文件 (*.*)\0*.*\0";
			ofn.lpstrFile = exePath;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
			if (!GetOpenFileNameW(&ofn)) return;
		}
		else {
			return;
		}
	}

	// 启动进程并注入组件，成功后关闭启动器窗口
	if (LaunchAndInject(exePath, dllPath)) {
		PostQuitMessage(0);
	}
	else {
		MessageBoxW(hwnd, L"启动失败，请以管理员身份运行。", L"错误", MB_ICONERROR);
	}
}

/**
 * 主窗口消息回调处理
 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE: {
		// 设置 UI 默认字体
		HFONT hFont = CreateFontW(
			-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei"
		);

		// 创建启动按钮
		HWND hBtn = CreateWindowW(
			L"BUTTON", L"启动",
			WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			40, 30, 200, 45,
			hwnd, (HMENU)(UINT_PTR)ID_BTN_LAUNCH, NULL, NULL
		);

		SendMessage(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
		break;
	}

	case WM_COMMAND:
		// 响应按钮点击
		if (LOWORD(wParam) == ID_BTN_LAUNCH) {
			HWND hBtn = GetDlgItem(hwnd, ID_BTN_LAUNCH);
			EnableWindow(hBtn, FALSE);
			SetWindowTextW(hBtn, L"启动中...");
			ExecuteLaunchSequence(hwnd);
			EnableWindow(hBtn, TRUE);
			SetWindowTextW(hBtn, L"启动");
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
	return 0;
}

/**
 * Windows 应用入口点
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	(void)hPrevInstance;
	(void)lpCmdLine;

	const wchar_t CLASS_NAME[] = L"LauncherClass";

	// 注册窗口类
	WNDCLASSW wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClassW(&wc);

	// 计算居中坐标
	int winWidth = 290;
	int winHeight = 145;
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int posX = (screenWidth - winWidth) / 2;
	int posY = (screenHeight - winHeight) / 2;

	// 创建无多余边框的居中主窗口
	HWND hwnd = CreateWindowExW(
		0, CLASS_NAME, L"原神启动器",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		posX, posY, winWidth, winHeight,
		NULL, NULL, hInstance, NULL
	);

	if (!hwnd) return 0;

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// 消息循环
	MSG msg = { 0 };
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return (int)msg.wParam;
}