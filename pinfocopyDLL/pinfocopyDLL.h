// �ȉ��� ifdef �u���b�N�� DLL ����̃G�N�X�|�[�g��e�Ղɂ���}�N�����쐬���邽�߂� 
// ��ʓI�ȕ��@�ł��B���� DLL ���̂��ׂẴt�@�C���́A�R�}���h ���C���Œ�`���ꂽ PINFOCOPYDLL_EXPORTS
// �V���{�����g�p���ăR���p�C������܂��B���̃V���{���́A���� DLL ���g�p����v���W�F�N�g�ł͒�`�ł��܂���B
// �\�[�X�t�@�C�������̃t�@�C�����܂�ł��鑼�̃v���W�F�N�g�́A 
// PINFOCOPYDLL_API �֐��� DLL ����C���|�[�g���ꂽ�ƌ��Ȃ��̂ɑ΂��A���� DLL �́A���̃}�N���Œ�`���ꂽ
// �V���{�����G�N�X�|�[�g���ꂽ�ƌ��Ȃ��܂��B
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