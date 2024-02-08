// IdiomDragonApp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <drogon/drogon.h>
#include <Windows.h>
#include <VersionHelpers.h>
#include <cstdio>
#include "../../resource/tool.h"

#include "server.h"
#include "ui.h"

HINSTANCE hInst;

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


void InitAuthTokenVerify(CmdLineW& cl);


// wWinMain function: The application will start from here.
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	// TODO: Place code here.
	UNREFERENCED_PARAMETER(0);
	using namespace std;

	if (!IsWindowsVistaOrGreater()) {
		fprintf(stderr, "[FATAL] Your OS is TOO LOW!\nIf you want to run this "
			"program, please update your OS.\nAt least Windows Vista is required.\n"
			"Exiting...\n");
		return ERROR_NOT_SUPPORTED; // Exit
	}


	::hInst = hInstance;


	CmdLineW cl(GetCommandLineW());
	wstring type;
	cl.getopt(L"type", type);


	if (type == L"server") {
		bool allow_global_access = (cl.getopt(L"allow-global-access") != 0);
		wstring sPort; unsigned short port = 0;
		cl.getopt(L"port", sPort);
		port = (unsigned short)atoi(ws2c(sPort));
		if (!port) return ERROR_INVALID_PARAMETER;

		std::wstring root = GetProgramDirW() + L".data";
		(cl.getopt(L"root-path", root));
		if (-1 != IsFileOrDirectory(root)) {
			if (!CreateDirectoryW(root.c_str(), NULL)) {
				return GetLastError();
			}
			//SetFileAttributesW(root.c_str(), FILE_ATTRIBUTE_HIDDEN);
		}
		SetCurrentDirectoryW(root.c_str());
		std::wstring webroot = root + L"/webroot";
		(cl.getopt(L"webroot", webroot));
		if (-1 != IsFileOrDirectory(webroot)) {
			if (!CreateDirectoryW(webroot.c_str(), NULL)) {
				return GetLastError();
			}
		}
		std::wstring uploadPath = root + L"/uploads";

		std::wstring sslCert, sslKey;
		cl.getopt(L"ssl-cert", sslCert);
		cl.getopt(L"ssl-key", sslKey);


		InitAuthTokenVerify(cl);

		string s_webroot, s_upload, s_sslCrt, s_sslKey;
		ConvertUTF16ToUTF8(webroot, s_webroot);
		ConvertUTF16ToUTF8(uploadPath, s_upload);
		ConvertUTF16ToUTF8(sslCert, s_sslCrt);


		std::shared_ptr<server::MainServer> srv(new server::MainServer);
		auto& app = drogon::app();
		app.setLogPath("./")
			.setLogLevel(trantor::Logger::kDebug)
			.setDocumentRoot(s_webroot)
			.setUploadPath(s_upload)
			.setThreadNum(8)
			.setClientMaxBodySize(2147483648)
			.setClientMaxMemoryBodySize(67108864)
			.setFileTypes({
				"html","css","js",
				"webmanifest","importmap",
				"png","json","svg","map"})
			.registerController(srv);
		bool useSSL = false;
		if (!sslCert.empty()) {
			useSSL = true;
			if (sslKey.empty()) sslKey = sslCert;
			ConvertUTF16ToUTF8(sslKey, s_sslKey);
			app.setSSLFiles(s_sslCrt, s_sslKey);
		}
		app.addListener(allow_global_access ? "0.0.0.0" : "127.0.0.1", port, useSSL);

		std::wstring s_signal; cl.getopt(L"signal", s_signal);
		if (!s_signal.empty()) {
			HANDLE signal = (HANDLE)(LONG_PTR)atoll(ws2s(s_signal).c_str());
			if (signal) {
				HANDLE hThread = CreateThread(0, 0, [](PVOID signal)->DWORD {
					HANDLE sign = (HANDLE)signal;
					WaitForSingleObject(sign, INFINITE);
					drogon::app().quit();
					return 0;
				}, signal, 0, 0);
				if (hThread) CloseHandle(hThread);
			}
		}

		app.run();

		return 0;
	}

	if (type == L"ui") {
		return UIEntry(cl);
	}

	
	if (cl.argc() < 2) {
		return Process.StartAndWait(L"\"" + GetProgramDirW() + L"\" --type=ui ");
	}

	return ERROR_INVALID_PARAMETER;
	return 0;
}




