#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

// 定数

#define			MAX_FILE_WRITE_LOCK_NUM			256


// TSファイル読み込みクラス

class	TSFileRead
{
public:
	__int64		totalread;
	__int64		readpoint;
	__int64		filesize;

	TSFileRead(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf);
	~TSFileRead();

	void			seek(__int64 fpoint);
	unsigned char*	read();
	void			showPacketError(BOOL bError);
	
private:
	HANDLE			hReadFile;

	unsigned int	bufsize;			// バッファ1面の容量
	unsigned int	bufnum;				// バッファの個数
	unsigned int	packetsize;			// パケットサイズ 188 or 192

	__int64			filepos;			// バッファ中のデータ先頭のファイルにおける位置，seek()で使用
	
	unsigned char	*buftop;			// バッファの先頭アドレス（実際は主にbuftop + bufsizeの位置から使用する）
	size_t			datsize;			// 今バッファ中にあるデータの容量 + バッファ1面分のゲタ
	size_t			datpoint;			// バッファ中の今読んでる場所 + バッファ1面分のゲタ

	BOOL			bFileEnd;			// ファイル終端まで読んだらTRUE
	BOOL			bNoBuf;				// no buffering有効(FILE_FLAG_NO_BUFFERING)ならTRUE
	BOOL			bPacketError;		// TRUEなら"不正な...削除しました."を表示する
};



// TSファイル書き出しクラス
// 一定サイズで自動書き込み

class	TSFileWriteAuto
{
public:
	__int64		totalwrite;
	__int64		writepoint;

	TSFileWriteAuto(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf);
	~TSFileWriteAuto();

	unsigned char*	write();
	void			finish();

private:
	HANDLE			hWriteFile;			// 書き込み用主ハンドル
	HANDLE			hWriteFileSub;		// 主ハンドルがFILE_FLAG_NO_BUFFERINGの場合のサブハンドル

	unsigned int	bufsize;			// バッファ1面の容量
	unsigned int	bufnum;				// バッファの個数
	size_t			totalbufsize;		// bufsize * bufnum;

	unsigned int	packetsize;			// パケットサイズ 188 or 192

	unsigned char	*buftop;			// バッファの先頭
	size_t			datsize;			// 今バッファ中にあるデータの容量

	BOOL			bNoBuf;				// no buffering有効(FILE_FLAG_NO_BUFFERING)ならTRUE

};


// TSファイル書き出しクラス
// 書き込みタイミング制御型(シングルロック)

class	TSFileWrite
{
public:
	__int64		totalwrite;
	__int64		writepoint;

	TSFileWrite(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf);
	~TSFileWrite();

	unsigned char*	write();
	void			lock();
	void			unlock();
	void			release();
	void			finish();

private:
	HANDLE			hWriteFile;			// 書き込み用主ハンドル
	HANDLE			hWriteFileSub;		// 主ハンドルがFILE_FLAG_NO_BUFFERINGの場合のサブハンドル

	unsigned int	bufsize;			// バッファ1面の容量
	unsigned int	bufnum;				// バッファの個数
	size_t			totalbufsize;		// bufsize * bufnum

	unsigned int	packetsize;			// パケットサイズ 188 or 192

	unsigned char	*buftop;			// バッファの先頭
	
	unsigned int	bufcount;			// 今データの先頭があるバッファの番号 0～(bufnum-1)
	size_t			datsize;			// 今バッファ中にあるデータの容量
	size_t			datpos;				// バッファ中の現在の書き込み位置
	unsigned int	exdat;				// バッファの最後からはみ出したデータの量	-> バッファ先頭にコピーされる

	BOOL			bLock;				// バッファからディスクへの書き出しがロックされているかどうか
	BOOL			bNoBuf;				// no buffering有効(FILE_FLAG_NO_BUFFERING)ならTRUE

};


// TSファイル書き出しクラス
// マルチロック制御型

class	TSFileWriteMultipleLock
{
public:
	__int64		totalwrite;
	__int64		writepoint;

	TSFileWriteMultipleLock(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf);
	~TSFileWriteMultipleLock();

	unsigned char*	write();
	void			lock(int uid);
	void			unlock(int uid);
	void			release(int uid);
	void			finish();

private:
	HANDLE			hWriteFile;				// 書き込み用主ハンドル
	HANDLE			hWriteFileSub;			// 主ハンドルがFILE_FLAG_NO_BUFFERINGの場合のサブハンドル

	unsigned int	bufsize;				// バッファ1面の容量
	unsigned int	bufnum;					// バッファの個数
	size_t			totalbufsize;			// bufsize * bufnum

	unsigned int	packetsize;				// パケットサイズ 188 or 192

	unsigned  char	*buftop;				// バッファの先頭
	
	unsigned int	bufcount;				// 今データの先頭があるバッファの番号 0～(bufnum-1)
	size_t			datsize;				// 今バッファ中にあるデータの容量
	size_t			datpos;					// バッファ中の現在の書き込み位置
	unsigned int	exdat;					// バッファの最後からはみ出したデータの量	-> バッファ先頭にコピーされる

	int				nLockCount;								// バッファからディスクへの書き出しをロックしているuidの数
	int				lockUid[MAX_FILE_WRITE_LOCK_NUM];		// ロックしているuid
	size_t			lockDataSize[MAX_FILE_WRITE_LOCK_NUM];	// ロックしているサイズ

	BOOL			bNoBuf;					// no buffering有効(FILE_FLAG_NO_BUFFERING)ならTRUE
};
