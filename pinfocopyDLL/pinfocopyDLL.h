// 以下の ifdef ブロックは DLL からのエクスポートを容易にするマクロを作成するための 
// 一般的な方法です。この DLL 内のすべてのファイルは、コマンド ラインで定義された PINFOCOPYDLL_EXPORTS
// シンボルを使用してコンパイルされます。このシンボルは、この DLL を使用するプロジェクトでは定義できません。
// ソースファイルがこのファイルを含んでいる他のプロジェクトは、 
// PINFOCOPYDLL_API 関数を DLL からインポートされたと見なすのに対し、この DLL は、このマクロで定義された
// シンボルをエクスポートされたと見なします。
#ifdef PINFOCOPYDLL_EXPORTS
#define PINFOCOPYDLL_API __declspec(dllexport)
#else
#define PINFOCOPYDLL_API __declspec(dllimport)
#endif


#ifdef  __cplusplus
extern "C" {
#endif

//PINFOCOPYDLL_API int __stdcall fnpinfocopyDLL(void);
PINFOCOPYDLL_API LPCWSTR __stdcall version();
PINFOCOPYDLL_API int __stdcall pinfocopy(LPCWSTR args);

#ifdef  __cplusplus
}
#endif