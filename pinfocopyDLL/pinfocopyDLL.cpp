// pinfocopyDLL.cpp : DLL アプリケーション用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include "pinfocopyDLL.h"
#include "resource.h"


// これは、エクスポートされた関数の例です。
//PINFOCOPYDLL_API int __stdcall fnpinfocopyDLL(void)
//{
//	return 42;
//}

_TCHAR* getVer();
int pinfocopy(int argc, _TCHAR* argv[]);

PINFOCOPYDLL_API LPCWSTR __stdcall version()
{
	static _TCHAR _version[256];
	_tcsset_s(_version,'\0',sizeof(_version));
	_tcscpy_s(_version, getVer());
	_tcscat_s(_version, L"(DLL Ver." VER_STR_PRODUCTVERSION L" " VER_STR_LEGALCOPYRIGHT L")");
	return _version;
}

PINFOCOPYDLL_API int __stdcall pinfocopy(LPCWSTR args)
{
	int argc = 1;
	_TCHAR* argv[256];
	_TCHAR args_t[2048];
	_TCHAR* next_token;

	_tcscpy_s(args_t, args);
	argv[argc] = _tcstok_s(args_t, L" \t\r\n", &next_token);
	while(argv[argc] != NULL)
	{
		argc++;
		argv[argc] = _tcstok_s( NULL, L" \t\r\n", &next_token);
	}

	return pinfocopy(argc, argv);
}