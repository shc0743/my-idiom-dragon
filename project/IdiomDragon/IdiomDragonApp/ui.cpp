#include "ui.h"
#include <Uxtheme.h>


extern HINSTANCE hInst;


static DWORD InitUIClass(HINSTANCE hInst);

static LRESULT CALLBACK WndProc_Main(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static WCHAR gwClassApp[256] = L"gwClassApp";
static HFONT hFontDefault;

static bool hasPendingOperation;
static HANDLE hEventCoreWorker;
static std::deque<unsigned long long> nCoreWorkerCode;

static HANDLE hServerProcess;
static std::wstring sInstanceDir;
static HANDLE hServerSignal;


static DWORD CALLBACK Thread_CoreWorker(PVOID data);




int __stdcall UIEntry(CmdLineW& cl) {
	SetCurrentDirectoryW(GetProgramPathW().c_str());

	if (DWORD ret = InitUIClass(hInst)) return ret;

	WCHAR appTitle[256] = L"成语接龙 服务进程";

	if (HWND hwnd = FindWindowW(gwClassApp, NULL)) {
		if (!cl.getopt(L"new-instance")) {
			ShowWindow(hwnd, SW_RESTORE);
			SetForegroundWindow(hwnd);
			return ~int(STILL_ACTIVE);
		}
	}

	cl.getopt(L"instance-dir", sInstanceDir);
	if (sInstanceDir.empty()) {
		sInstanceDir = GetProgramDirW() + L".data";
		if (-1 != IsFileOrDirectory(sInstanceDir)) {
			if (!CreateDirectoryW(sInstanceDir.c_str(), NULL)) {
				return GetLastError();
			}
			//SetFileAttributesW(sInstanceDir.c_str(), FILE_ATTRIBUTE_HIDDEN);
		}
	}
	SetCurrentDirectoryW(sInstanceDir.c_str());

	HANDLE hLockFile = NULL;
	{
		DWORD pid = GetCurrentProcessId(), temp = 0;
		hLockFile = CreateFileW(L".lockfile", FILE_READ_DATA,
			FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL , NULL);
		if (hLockFile != INVALID_HANDLE_VALUE) {
			DWORD targetPID = 0;
			if (ReadFile(hLockFile, &targetPID, sizeof(DWORD), &temp, NULL)) {
				if (targetPID != pid && !cl.getopt(L"force")) {
					HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, targetPID);
					if (hProcess && (MessageBoxW(NULL,
						(std::wstring(L"A instance in the directory is still running [PID=].\n"
						"If you run a new instance in the same directory, "
						"it may cause unpredictable consequences.\n"
						"If you want to run a new instance without conflict, "
						"please use --instance-dir command-line option.\n"
						"\nStill continue?").insert(50, std::to_wstring(targetPID))).c_str(),
						L"Instance Conflict",
						MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2) != IDOK)) {
						CloseHandle(hProcess);
						CloseHandle(hLockFile);
						return ERROR_ALREADY_EXISTS;
					}
					if (hProcess) CloseHandle(hProcess);
				}
			}
			CloseHandle(hLockFile);
		}
		hLockFile = CreateFileW(L".lockfile",
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, 0, NULL);
		if (hLockFile) {
			WriteFile(hLockFile, &pid, sizeof(DWORD), &temp, NULL);
			FlushFileBuffers(hLockFile);
		}
		if (hLockFile) CloseHandle(hLockFile);
	}

	HWND hwnd = CreateWindowExW(0, gwClassApp, appTitle, WS_OVERLAPPEDWINDOW,
		0, 0, 500, 320, 0, 0, 0, 0);
	if (!hwnd) return GetLastError();

	CenterWindow(hwnd);
	if (!cl.getopt(L"silent")) ShowWindow(hwnd, SW_NORMAL);

	if (HANDLE hThread = CreateThread(0, 0, Thread_CoreWorker, hwnd, CREATE_SUSPENDED, 0)) {
		ResumeThread(hThread);
		CloseHandle(hThread);
	} else {
		MessageBoxW(NULL, LastErrorStrW().c_str(), NULL, MB_ICONHAND);
		return GetLastError();
	}


	//HACCEL hAcc = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR_MAINAPP));

	MSG msg{};
	while (GetMessageW(&msg, 0, 0, 0)) {
		//if (!TranslateAccelerator(msg.hwnd, hAcc, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		//}
	}
	DeleteFileW(L".lockfile");
	return (int)msg.wParam;
	return 0;
}


static DWORD InitUIClass(HINSTANCE hInst) {
	hFontDefault = CreateFontW(-14, -7, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
		OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, FF_DONTCARE,
		L"Consolas");

	INITCOMMONCONTROLSEX icce{};
	icce.dwSize = sizeof icce;
	icce.dwICC = ICC_ALL_CLASSES;
	if (!InitCommonControlsEx(&icce)) return GetLastError();

	/*if (!LoadStringW(hInst, IDS_STRING_UI_CLASS_MAIN, gwClassApp, 256)) return GetLastError();
	if (!LoadStringW(hInst, IDS_STRING_UI_CLASS_CREDIT, gwClassCredit, 256)) return GetLastError();*/

	if (!s7::MyRegisterClassW(gwClassApp, WndProc_Main, {
		.hInstance = hInst,
		.hbrBackground = CreateSolidBrush(RGB(255,255,255)),
		//.lpszMenuName = MAKEINTRESOURCE(IDR_MENU_MAINAPP),
	})) return GetLastError();

	return 0;
}




struct WndData_Main
{
	HWND
		tServerStatus,
		bToggleServer,
		Split1,
		cAllowGlobAccess,
		tServerPort,
		eServerPort,
		tServerAddr,
		eServerAddr,
		bServerAddr,
		bCopySaddr,
		Split2,
		Split3,
		tSSL, eSSL, bSSL,
		tSSLK, eSSLK, bSSLK, bSSLGen,
		Split4,
		bEditUser /*8*/;
};
struct WndData_CreditEdit
{
	HWND
		eAddText{},
		bAdd{}, bDelete{}, bClear{},
		List{};
	std::vector<std::wstring> credits;
};


std::string ossl_sha256(const std::string str);


inline void NotifyCoreWorker(unsigned long long code) {
	::nCoreWorkerCode.push_back(code);
	SetEvent(::hEventCoreWorker);
}
DWORD Thread_CoreWorker(PVOID dd) {
	using namespace std;
	HWND hwnd = (HWND)dd;
	if (!IsWindow(hwnd)) return ERROR_NOT_FOUND;

	WndData_Main* data = (WndData_Main*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	const auto enable = [data] (BOOL bEnable) {
		EnableWindow(data->bToggleServer, bEnable);
		EnableWindow(data->cAllowGlobAccess, bEnable);
		EnableWindow(data->eServerPort, bEnable);
		EnableWindow(data->eSSL, bEnable);
		EnableWindow(data->bSSL, bEnable);
		EnableWindow(data->eSSLK, bEnable);
		EnableWindow(data->bSSLK, bEnable);
		EnableWindow(data->bEditUser, bEnable);
	};

	SetWindowTextW(data->tServerStatus, L"stopped");
	enable(TRUE);

	::hEventCoreWorker = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!hEventCoreWorker) return GetLastError();
	unsigned long long code = 0;
	do {
		WaitForSingleObject(hEventCoreWorker, INFINITE);

		while (nCoreWorkerCode.size() > 0) {
			code = ::nCoreWorkerCode[0];
			::nCoreWorkerCode.pop_front();
			if (code == WM_QUIT) break;

			switch (code)
			{

			case 0x10001: {
				if (::hServerProcess) {
					SetWindowTextW(data->bToggleServer, L"Stopping Server");
					wstring errText = L"Cannot stop the server. Do you want to terminate it?";
					if (hServerSignal) {
						DWORD code = 0;
						SetEvent(hServerSignal);
						WaitForSingleObject(hServerProcess, 5000);
						GetExitCodeProcess(hServerProcess, &code);
						if (code == STILL_ACTIVE) {
							errText = L"It seems like the server has not response. "
								"Do you want to terminate it?";
							goto err;
						}
						goto ok;
					}

				err:
					if (MessageBoxW(hwnd, errText.c_str(), L"Stop Server", MB_OKCANCEL) == IDOK) {
						if (TerminateProcess(hServerProcess, ERROR_TIMEOUT)) goto ok;
						if (Process.kill(hServerProcess, ERROR_TIMEOUT)) goto ok;
						auto k32 = GetModuleHandle(TEXT("kernel32.dll"));
						if (k32) {
							auto f = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, "ExitProcess");
#pragma warning(push)
#pragma warning(disable: 6001)
							HANDLE hRemote = CreateRemoteThread(::hServerProcess,
								0, 0, f, (PVOID)ERROR_TIMEOUT, 0, 0);
							if (hRemote) {
								CloseHandle(hRemote); goto ok;
							}
#pragma warning(pop)
						}
						MessageBoxW(hwnd, (L"FATAL: CANNOT STOP SERVER\n" +
							LastErrorStrW()).c_str(), L"Cannot Stop Server", MB_ICONHAND);
					}
					EnableWindow(data->bToggleServer, TRUE);
					SetWindowTextW(data->bToggleServer, L"Stop Server");
					break;

				ok:
					if (hServerSignal) CloseHandle(hServerSignal);
					hServerSignal = NULL;

					break;
				}

				enable(FALSE);

				WCHAR port[7]{};
				GetWindowTextW(data->eServerPort, port, 7);
				std::wstring cl = L"Web-File-Explorer --type=server --port=\""s + port + L"\" ";
				if (SendMessage(data->cAllowGlobAccess, BM_GETSTATE, 0, 0) & BST_CHECKED) {
					cl += L"--allow-global-access ";
				}
				if (!file_exists(L"users.data")) try {
					fstream fp("users.data", ios::out);
					fp << endl;
					fp.close();
				}
				catch (...) {
					MessageBoxW(hwnd, L"用户数据初始化失败！", 0, MB_ICONHAND);
				}
				cl += L"--token=File;users.data ";

				cl += L"--root-path=\"" + sInstanceDir + L"\" ";

				WCHAR sslOpt[1024]{}; bool bHttpsEnabled = false;
				GetWindowTextW(data->eSSL, sslOpt, 1024);
				if (sslOpt[0]) {
					bHttpsEnabled = true;
					cl += L"--ssl-cert=\"" + std::wstring(sslOpt) + L"\" --ssl-key=\"";
					GetWindowTextW(data->eSSLK, sslOpt, 1024);
					cl += std::wstring(sslOpt) + L"\" ";
				}

				SECURITY_ATTRIBUTES sa{};
				sa.nLength = sizeof(sa);
				sa.bInheritHandle = TRUE;
				hServerSignal = CreateEvent(&sa, FALSE, FALSE, NULL);
				if (hServerSignal) {
					cl += L"--signal=" + std::to_wstring((LONG_PTR)(PVOID)hServerSignal) + L" ";
				}

				STARTUPINFOW si{}; PROCESS_INFORMATION pi{};
				si.cb = sizeof(si);
				PWSTR mem = (PWSTR)calloc(cl.length() + 1, sizeof(WCHAR));
				if (!mem) {
					MessageBoxW(hwnd, LastErrorStrW().c_str(), NULL, MB_ICONERROR);
					enable(TRUE);
					EnableWindow(data->bServerAddr, FALSE);
					break;
				}
				wcscpy_s(mem, cl.length() + 1, cl.c_str());
				if (!(CreateProcessW(GetProgramDirW().c_str(), mem, NULL, NULL,
					TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi) && pi.hProcess)) {
					free(mem);
					MessageBoxW(hwnd, LastErrorStrW().c_str(), NULL, MB_ICONERROR);
					enable(TRUE);
					EnableWindow(data->bServerAddr, FALSE);
					break;
				}
				free(mem);
				::hServerProcess = pi.hProcess;

				HANDLE hThread = NULL;
				const auto lambda1 = [](PVOID d) -> DWORD {
					DWORD c = 0;
					WaitForSingleObject(d, INFINITE);
					GetExitCodeProcess(d, &c);

					NotifyCoreWorker(0x10002);
					return 0;
				};
				hThread = CreateThread(0, 0, lambda1, pi.hProcess, 0, 0);
#pragma warning(push)
#pragma warning(disable: 6001)
				if (hThread) CloseHandle(hThread);
#pragma warning(pop)

				std::wstring srv_addr = bHttpsEnabled ? L"https" : L"http";
				srv_addr += L"://127.0.0.1:"; srv_addr += port;
				SetWindowTextW(data->eServerAddr, srv_addr.c_str());

				SetWindowTextW(data->tServerStatus, L"running");
				SetWindowTextW(data->bToggleServer, L"Stop Server");
				EnableWindow(data->bToggleServer, TRUE);
				EnableWindow(data->bServerAddr, TRUE);

				ResumeThread(pi.hThread);
				CloseHandle(pi.hThread);
			}
				break;

			case 0x10002: {
				CloseHandle(hServerProcess);
				::hServerProcess = NULL;
				SetWindowTextW(data->tServerStatus, L"stopped");
				SetWindowTextW(data->bToggleServer, L"Start Server");
				SetWindowTextW(data->eServerAddr, L"N/A");
				enable(TRUE);
				EnableWindow(data->bServerAddr, FALSE);
			}
				break;


			default:;
			}
		}
	} while (code != WM_QUIT);

	return 0;
}



static LRESULT WndProc_CommandHandler_Main(HWND hwnd, WPARAM wParam, LPARAM lParam, WndData_Main* data);
LRESULT WndProc_Main(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	WndData_Main* data = (WndData_Main*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (message)
	{
	case WM_CREATE: {
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		SetLayeredWindowAttributes(hwnd, NULL, 0xF0, LWA_ALPHA);

		DragAcceptFiles(hwnd, TRUE);

		WndData_Main* data = new(WndData_Main);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);

#define MYCTLS_VAR_HWND hwnd
#define MYCTLS_VAR_HINST hInst
#define MYCTLS_VAR_HFONT hFontDefault
#include "ctls.h"
using namespace std;

		text(L"Server status:", 10, 10, 100, 20, SS_CENTERIMAGE);
		data->tServerStatus = text(L"unknown", 0, 0, 1, 1, SS_CENTERIMAGE);
		data->bToggleServer = button(L"Start Server", 1, 0, 0, 1, 1, WS_DISABLED);
		data->Split1 = text(L"", 0, 0, 1, 1, WS_BORDER);
		data->cAllowGlobAccess =
			button(L"Allow LAN Access", 0, 0, 0, 1, 1, WS_DISABLED | BS_AUTOCHECKBOX);
		data->tServerPort = text(L"Server Port:", 0, 0, 1, 1, SS_CENTERIMAGE);
		data->eServerPort = edit(L"28314", 0, 0, 1, 1, WS_DISABLED | ES_NUMBER);
		data->tServerAddr = text(L"Server Address:", 0, 0, 1, 1, SS_CENTERIMAGE);
		data->eServerAddr = edit(L"N/A (server not active)", 0, 0, 1, 1, ES_READONLY);
		data->bCopySaddr = button(L"Copy", 2);
		data->bServerAddr = button(L"Open", 3, 0, 0, 1, 1, WS_DISABLED);
		data->Split2 = text(L"", 0, 0, 1, 1, WS_BORDER);
		//data->Split3 = text(L"", 0, 0, 1, 1, WS_BORDER);
		data->tSSL = text(L"SSL Cert:", 0, 0, 1, 1, SS_CENTERIMAGE);
		data->eSSL = edit(L"");
		data->bSSL = button(L"Choose", 5, 0, 0, 1, 1, WS_DISABLED);
		data->tSSLK = text(L"SSL Key:", 0, 0, 1, 1, SS_CENTERIMAGE);
		data->eSSLK = edit(L"");
		data->bSSLK = button(L"Choose", 6, 0, 0, 1, 1, WS_DISABLED);
		data->bSSLGen = button(L"Generate", 7);
		data->Split4 = text(L"", 0, 0, 1, 1, WS_BORDER);
		data->bEditUser = button(L"用户管理...", 8, 0, 0, 1, 1, WS_DISABLED);

		SendMessage(data->cAllowGlobAccess, BM_SETCHECK, BST_CHECKED, 0);


#undef MYCTLS_VAR_HFONT
#undef MYCTLS_VAR_HINST
#undef MYCTLS_VAR_HWND
	}
		break;

	case WM_SIZING:
	case WM_SIZE:
	{
		RECT rc{}; GetClientRect(hwnd, &rc);
		SIZE sz{ .cx = rc.right - rc.left,.cy = rc.bottom - rc.top };

		SetWindowPos(data->tServerStatus,	0,	120, 10, sz.cx -20 -110, 20, 0);
		SetWindowPos(data->bToggleServer,	0,	10, 40, sz.cx -20, 40, 0);
		SetWindowPos(data->Split1,			0,	10, 90, sz.cx -20, 1, 0);
		SetWindowPos(data->cAllowGlobAccess,0,	10, 100, 160, 20, 0);
		SetWindowPos(data->tServerPort,		0,	180, 100, 90, 20, 0);
		SetWindowPos(data->eServerPort,		0,	280, 100, sz.cx -290, 20, 0);
		SetWindowPos(data->tServerAddr,		0,	10, 130, 110, 20, 0);
		SetWindowPos(data->eServerAddr,		0,	130, 130, sz.cx -280, 20, 0);
		SetWindowPos(data->bCopySaddr,		0,	sz.cx -140, 130, 60, 20, 0);
		SetWindowPos(data->bServerAddr,		0,	sz.cx -70, 130, 60, 20, 0);
		SetWindowPos(data->Split2,			0,	10, 160, sz.cx -20, 1, 0);
		//SetWindowPos(data->Split3,			0,	10, 210, sz.cx -20, 1, 0);
		SetWindowPos(data->tSSL,			0,	10, 170, 60, 20, 0);
		SetWindowPos(data->eSSL,			0,	80, 170, sz.cx -250, 20, 0);
		SetWindowPos(data->bSSL,			0,	sz.cx -160, 170, 70, 20, 0);
		SetWindowPos(data->tSSLK,			0,	10, 200, 60, 20, 0);
		SetWindowPos(data->eSSLK,			0,	80, 200, sz.cx -250, 20, 0);
		SetWindowPos(data->bSSLK,			0,	sz.cx -160, 200, 70, 20, 0);
		SetWindowPos(data->bSSLGen,			0,	sz.cx -80, 170, 70, 50, 0);
		SetWindowPos(data->Split4,			0,	10, 230, sz.cx -20, 1, 0);
		SetWindowPos(data->bEditUser,		0,	10, 240, sz.cx -20, sz.cy -250, 0);

	}
		break;

	case WM_COMMAND:
		return WndProc_CommandHandler_Main(hwnd, wParam, lParam, data);
		break;

	case WM_CLOSE:
		if (hasPendingOperation) {
			MessageBoxTimeoutW(hwnd, L"Pending operation detected", NULL, MB_ICONERROR, 0, 1000);
			return FALSE;
		}
		if (::hServerProcess) {
			MessageBoxTimeoutW(hwnd, L"Server is still running", NULL, MB_ICONERROR, 0, 1000);
			return FALSE;
		}
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		NotifyCoreWorker(WM_QUIT);
		if (data) delete data;
		PostQuitMessage(0);
		break;

	default: return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}
LRESULT WndProc_CommandHandler_Main(HWND hwnd, WPARAM wParam, LPARAM lParam, WndData_Main* data) {
	int wmId = LOWORD(wParam);
	switch (wmId)
	{
	case 1:
		EnableWindow(data->bToggleServer, FALSE);
		NotifyCoreWorker(0x10001);
		break;

	case 2:
	case 3:
	{
		WCHAR buffer[256]{};
		if (wmId == 2) {
			GetWindowTextW(data->eServerAddr, buffer, 256);
			auto mb = MessageBoxTimeoutW;
			return (CopyText(buffer) == 0) ?
				mb(hwnd, L"Copied", L"Success", MB_ICONINFORMATION, 0, 500) :
				mb(hwnd, L"Failed to copy", NULL, MB_ICONERROR, 0, 500);
		}
		GetWindowTextW(data->eServerAddr, buffer, 256);
		ShellExecuteW(NULL, L"open", buffer, NULL, NULL, SW_NORMAL);
	}
		break;

	case 5:
	case 6:
	case 7:
	{
		WCHAR wsFile[1024]{};
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof ofn;
		ofn.hwndOwner = hwnd;
		ofn.lpstrTitle = L"Choose File";
		ofn.lpstrFile = wsFile;
		ofn.nMaxFile = 1024;
		ofn.lpstrFilter = L"All Files\0*.*\0\0";
		ofn.Flags = OFN_EXPLORER;

		if (wmId == 7) {
			ofn.lpstrTitle = L"Please choose OpenSSL binary";
			ofn.lpstrFilter = L"OpenSSL\0openssl.exe\0executable\0*.exe\0\0";
		}

		if (!GetOpenFileNameW(&ofn)) break;

		switch (wmId) {
		case 5:
		case 6:
			SetWindowTextW(wmId == 5 ? data->eSSL : data->eSSLK, wsFile);
			break;

		case 7:
		{
			std::wstring ossl = wsFile, key, cert;
			ofn.lpstrFilter = L"PEM Key\0*.pem\0All Files\0*.*\0\0";
			wsFile[0] = L'\0';
			ofn.lpstrTitle = L"Please choose where to save the private key";
			if (!GetSaveFileNameW(&ofn)) break;
			key = wsFile;

			ofn.lpstrFilter = L"certificate\0*.cer\0All Files\0*.*\0\0";
			wsFile[0] = L'\0';
			ofn.lpstrTitle = L"Please choose where to save the public cert";
			if (!GetSaveFileNameW(&ofn)) break;
			cert = wsFile;

			std::wstring releasePath = L"_generate_ssl_cert.cmd";
			if (!FreeResFile(0, L"BIN", releasePath, hInst)) {
				MessageBoxW(hwnd, LastErrorStrW().c_str(), 0, MB_ICONERROR); break;
			}
			std::wstring command =
				L" \"" + ossl + L"\" \"" + key + L"\" \"" + cert + L"\"";
			str_replace(command, L"\\", L"/");
			Process.StartOnly(L"\"" + releasePath + L"\"" + command);
		}
			break;

		default:;
		}
	}
		break;

	case 8: {
		using namespace std;
		static HMENU h = NULL; if (!h) {
			h = CreatePopupMenu();
			if (!h) break;
			AppendMenuW(h, MF_STRING, 1, L"添加用户 (&A)");
			AppendMenuW(h, MF_STRING, 2, L"删除用户 (&D)");
			AppendMenuW(h, MF_STRING, 3, L"更改密码 (&C)");
		}
			
		RECT rc{}; GetWindowRect(data->bEditUser, &rc);
		int nResult = TrackPopupMenu(h, TPM_RETURNCMD | TPM_RIGHTBUTTON, rc.left, rc.bottom, 0, hwnd, NULL);
		//DestroyMenu(h);

		if (!nResult) break;
		AllocConsole();
		wchar_t buffer[256]{}; DWORD n = 0;
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), L"请输入用户名: ", 8, &n, 0);
		ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buffer, 256, &n, 0);
		if (n) do {
			wstring uname = buffer, pswd;
			uname.erase(uname.find_last_of(L"\r\n") - 1);
			if (uname.find(L"=") != uname.npos) {
				MessageBoxW(hwnd, L"用户名无效！", 0, MB_ICONHAND);
				break;
			}
			if (nResult == 3) {
				char buffer[256]{};
				char c = 0, a = 1;
				string sha1, sha2;
				b:
				WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), a ? 
					L"请输入新密码: " : L"请再次输入新密码", 8, &n, 0);
				while ((c = _getch()) && (c != '\r' && c != '\n')) {
					if (c == '\b')
						if (buffer[0]) buffer[strlen(buffer) - 1] = 0;
						else _putch('\a');
					else buffer[strlen(buffer)] = c;
				}
				_putch('\n');
				if (a) {
					sha1 = ossl_sha256(buffer);
					ZeroMemory(buffer, sizeof(buffer));
					a = 0; goto b;
				}
				sha2 = ossl_sha256(buffer);
				if (sha1 == sha2) {
					string s = "SHA256(value) == " + sha1 + "\n";
					WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), s.c_str(),
						(DWORD)s.length(), &n, 0);
					pswd = s2ws(sha1);
				}
				else {
					MessageBoxW(hwnd, L"密码不匹配！", 0, MB_ICONHAND); break;
				}
			}
			//vector<wstring> users;
			uname += L"=";
			wstring iii = uname + pswd;
			string iii8 = ConvertUTF16ToUTF8(iii);
			fstream fp(L"users.data", ios::in | ios::out);
			fstream fp2;
			if (!fp) {
				MessageBoxW(hwnd, LastErrorStr().c_str(), L"打不开文件", MB_ICONHAND); break;
			}
			if (nResult == 1) {
				iii8 += "\n";
				fp.seekg(0, ios::end);
				fp.write(iii8.c_str(), iii8.size());
				fp.close();
			}
			else {
				fp2.open(L"users.data.swap", ios::out);
				if (!fp2) {
					MessageBoxW(hwnd, LastErrorStr().c_str(), L"打不开文件", MB_ICONHAND); break;
				}
				char buffer[2048]{};
				while (fp.getline(buffer, 2048)) {
					wstring i = ConvertUTF8ToUTF16(buffer);
					if (i.starts_with(uname)) {
						if (nResult == 2) continue;
						i = uname + pswd;
					}
					string ii = ConvertUTF16ToUTF8(i) + "\n";
					fp2.write(ii.c_str(), ii.size());
				}
				fp2.close();
				fp.close();
				DeleteFileW(L"users.data");
				MoveFileW(L"users.data.swap", L"users.data");
			}
			MessageBox(hwnd, ErrorCodeToString(0).c_str(), L"OK", MB_ICONINFORMATION);
		} while (0);
		FreeConsole();
	}
		  break;

	default: return DefWindowProc(hwnd, WM_COMMAND, wParam, lParam);
	}
	return 0;
}











