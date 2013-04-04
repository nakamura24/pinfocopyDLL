#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

#include "tsinout.h"


//
//
// TSファイル読み込みクラス 
// TSFileReadのメンバ定義

TSFileRead::TSFileRead(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hReadFile = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

		if(hReadFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("読み込みファイルをFILE_FLAG_NO_BUFFERINGで開くのに失敗しました. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hReadFile = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hReadFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("読み込みファイルを開くのに失敗しました. error code %d.\n"), GetLastError());
			_tprintf(_T("読み込みファイルを開くのに失敗しました.\n"));
			exit(1);
		}
	}

	bPacketError = TRUE;

	bufsize    = bsize;
	bufnum     = bnum;
	packetsize = psize;

	LARGE_INTEGER	fsize;
	GetFileSizeEx(hReadFile, &fsize);
	filesize = fsize.QuadPart;

	totalread = 0;
	readpoint = 0;

	filepos = -1;		// まだデータ未読

	buftop = (unsigned char*)_aligned_malloc((size_t)bufsize * (bufnum + 1), (size_t)bufsize);
//	buftop = (unsigned char*)VirtualAlloc(NULL, (size_t)bufsize * (bufnum + 1), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	if(buftop == NULL){
		_tprintf(_T("読み込みバッファメモリの確保に失敗しました.\n"));
		exit(1);
	}

}

TSFileRead::~TSFileRead()
{
	CloseHandle(hReadFile);

	_aligned_free(buftop);
//	VirtualFree(buftop, 0, MEM_RELEASE);
	
}

void TSFileRead::seek(__int64 fpoint)
{
	BOOL			bResult;

	if( (fpoint < 0) || (fpoint >= filesize) ) {
		_tprintf(_T("データ読み込み中にエラーが発生しました.(データ範囲外の読み込み)\n"));
		exit(1);
	}

	if( (filepos == -1) || (filepos > fpoint) || ( (filepos + (__int64)bufsize * (__int64)bufnum) <= fpoint) )
	{
		// まだバッファにデータを未読の場合、バッファにあるデータ範囲外にseekした場合

		filepos = fpoint - (fpoint % bufsize);

		LARGE_INTEGER	fbase;
		fbase.QuadPart = filepos;
		SetFilePointerEx(hReadFile, fbase, NULL, FILE_BEGIN);

		datpoint = (fpoint % bufsize) + bufsize;

		bFileEnd  = FALSE;
		datsize   = bufsize;		// bufsize分のゲタ
		totalread = 0;
		readpoint = fpoint;

		unsigned int		i;
		DWORD				numread;

		for(i = 1; i <= bufnum; i++) {
			bResult = ReadFile(hReadFile, buftop + (size_t)i * bufsize, bufsize, &numread, NULL);

			if(!bResult){
				_tprintf(_T("データ読み込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}
			
			datsize += numread;
			if(numread != bufsize) {
				bFileEnd = TRUE;
				break;
			}
		}
	}
	else
	{
		// バッファに存在するデータ範囲内にseekする場合

		datpoint = (size_t)(fpoint - filepos + bufsize);

		totalread = 0;
		readpoint = fpoint;
	}

}

unsigned char* TSFileRead::read()
{
	BOOL		bResult;

	size_t		remain = datsize - datpoint;

	if( remain < packetsize ) {							// バッファ中のデータ残りがpacketsize未満になった時

		if(bFileEnd) {									// 既にファイル終端まで読み終わっている場合
			totalread += remain;
			readpoint += remain;
			return	NULL;								// 終了サインを返す
		}

		memcpy(buftop + bufsize - remain, buftop + datpoint, remain);					// データ半端分をバッファ前にコピー
		datpoint = bufsize - remain;
		datsize  = bufsize;

		unsigned int		i;
		DWORD				numread;

		filepos += ( (__int64)bufsize * bufnum);

		for(i = 1; i <= bufnum; i++) {
			bResult = ReadFile(hReadFile, buftop + (size_t)i * bufsize, bufsize, &numread, NULL);			// 残りをファイルから読み込み

			if(!bResult){
				_tprintf(_T("データ読み込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			datsize += numread;
			if(numread != bufsize) {
				bFileEnd = TRUE;
				break;
			}
		}

		if( (datsize - datpoint) < packetsize ) {										// 残りのデータを読み込んでもpacketsize未満しか無い時
			totalread += (datsize - datpoint);
			readpoint += (datsize - datpoint);
			return	NULL;																// 終了サインを返す
		}
	}


	unsigned int	h_offset = packetsize - 188;

	if(buftop[datpoint + h_offset] == 0x47) {											// パケットヘッダ0x47が正しくあった場合
		size_t	cdatpoint = datpoint;
		datpoint  += packetsize;
		totalread += packetsize;
		readpoint += packetsize;
		return	buftop + cdatpoint;														// そのポインタを返す
	}


	// パケットヘッダ0x47を見失った場合、先頭を探す

	unsigned int	resyncsize = packetsize * 2 + 1 + h_offset;

	__int64			removedat = 0;

//	_tprintf(_T("sync lost.\n"));

	while(1)
	{
		remain = datsize - datpoint;

		if(remain < resyncsize) {

			if(bFileEnd) {																	// 既にファイル終端まで読み終わっている場合
				totalread += remain;
				readpoint += remain;
				return	NULL;																// 終了サインを返す
			}

			memcpy(buftop + bufsize - remain, buftop + datpoint, remain);					// データ半端分をバッファ前にコピー
			datpoint = bufsize - remain;
			datsize  = bufsize;

			unsigned int		i;
			DWORD				numread;

			filepos += ( (__int64)bufsize * bufnum);

			for(i = 1; i <= bufnum; i++) {
				bResult = ReadFile(hReadFile, buftop + (size_t)i * bufsize, bufsize, &numread, NULL);			// 残りをファイルから読み込み

				if(!bResult){
					_tprintf(_T("データ読み込み中にエラーが発生しました. error code %d.\n"), GetLastError());
					exit(1);
				}

				datsize += numread;
				if(numread != bufsize) {
					bFileEnd = TRUE;
					break;
				}
			}

			if( (datsize - datpoint) < resyncsize) {										// 残りのデータを読み込んでもresyncsize以下しか無い時
				totalread += (datsize - datpoint);
				readpoint += (datsize - datpoint);
				return	NULL;																// 終了サインを返す
			}
		}

		if( (buftop[datpoint + h_offset] == 0x47) && (buftop[datpoint + packetsize + h_offset] == 0x47) && (buftop[datpoint + packetsize * 2 + h_offset] == 0x47) ) {
			break;		// 0x47 syncを回復したらbreak
		}

		datpoint++;
		removedat++;
		totalread++;
		readpoint++;
	}


	// 0x47 sync回復

	if(bPacketError) _tprintf(_T("%I64dバイトの不正なデータを削除しました.\n"), removedat);

	size_t	cdatpoint = datpoint;
	datpoint  += packetsize;
	totalread += packetsize;
	readpoint += packetsize;

	return	buftop + cdatpoint;															// そのポインタを返す

}

void TSFileRead::showPacketError(BOOL bError)
{
	bPacketError = bError;

	return;
}


//
//
// TSファイル書き込みクラス
// TSFileWriteAutoのメンバ定義

TSFileWriteAuto::TSFileWriteAuto(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hWriteFile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);

		if(hWriteFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("保存先ファイルをFILE_FLAG_NO_BUFFERINGで作成するのに失敗しました. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hWriteFile = CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("保存先ファイルを作成するのに失敗しました. error code %d.\n"), GetLastError());
			_tprintf(_T("保存先ファイルを作成するのに失敗しました.\n"));
			exit(1);
		}
	}
	else
	{
		hWriteFileSub = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFileSub == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("保存先ファイルを開くのに失敗しました. error code %d.\n"), GetLastError());
			_tprintf(_T("保存先ファイルを開くのに失敗しました.\n"));
			exit(1);
		}
	}

	bufsize    = bsize;
	bufnum     = bnum;
	packetsize = psize;

	totalbufsize = (size_t)bufsize * bufnum;

	datsize    = 0;
	totalwrite = 0;
	writepoint = 0;

	buftop = (unsigned char*)_aligned_malloc((size_t)bufsize * (bufnum + 1), (size_t)bufsize);
//	buftop = (unsigned char*)VirtualAlloc(NULL, (size_t)bufsize * (bufnum + 1), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	if(buftop == NULL){
		_tprintf(_T("書き込みバッファメモリの確保に失敗しました.\n"));
		exit(1);
	}

}

TSFileWriteAuto::~TSFileWriteAuto()
{
	if(bNoBuf)
	{
		CloseHandle(hWriteFileSub);
	} else {
		CloseHandle(hWriteFile);
	}

	_aligned_free(buftop);
//	VirtualFree(buftop, 0, MEM_RELEASE);

}

unsigned char* TSFileWriteAuto::write()
{
	BOOL			bResult;

	if(datsize >= totalbufsize) {

		unsigned int	i;
		DWORD			numwrite;

		for(i = 0; i < bufnum; i++) {
			bResult = WriteFile(hWriteFile, buftop + (size_t)i * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
		}

		memcpy(buftop, buftop + totalbufsize, datsize - totalbufsize);
		datsize = datsize - totalbufsize;
	}

	size_t	cdatsize = datsize;
	datsize += packetsize;
	writepoint += packetsize;

	return	buftop + cdatsize;
}


void TSFileWriteAuto::finish()
{
	BOOL			bResult;

	unsigned int	bcount = (unsigned int)(datsize / (size_t)bufsize);
	unsigned int	bmod   = (unsigned int)(datsize % (size_t)bufsize);

	unsigned int	i;
	DWORD			numwrite;

	for(i = 0; i < bcount; i++) {
			bResult = WriteFile(hWriteFile, buftop + (size_t)i * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
	}

	if(bNoBuf)
	{
		if(bmod != 0) {
			bResult = WriteFile(hWriteFile, buftop + (size_t)bcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			CloseHandle(hWriteFile);

			int		back_H = -1;
			int		back_L = (int)bmod - (int)bufsize; 

			SetFilePointer(hWriteFileSub, back_L, (PLONG)&back_H, FILE_END);
			SetEndOfFile(hWriteFileSub);
			totalwrite += back_L;
		}
		else
		{
			CloseHandle(hWriteFile);
		}
	}
	else
	{
		bResult = WriteFile(hWriteFile, buftop + (size_t)bcount * bufsize, bmod, &numwrite, NULL);

		if(!bResult){
			_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += bmod;
	}

}


//
// TSファイル書き込みクラス
// TSFileWriteのメンバ定義

TSFileWrite::TSFileWrite(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hWriteFile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);

		if(hWriteFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("保存先ファイルをFILE_FLAG_NO_BUFFERINGで作成するのに失敗しました. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hWriteFile = CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("保存先ファイルを作成するのに失敗しました. error code %d.\n"), GetLastError());
			_tprintf(_T("保存先ファイルを作成するのに失敗しました.\n"));
			exit(1);
		}
	}
	else
	{
		hWriteFileSub = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFileSub == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("保存先ファイルを開くのに失敗しました. error code %d.\n"), GetLastError());
			_tprintf(_T("保存先ファイルを開くのに失敗しました.\n"));
			exit(1);
		}
	}

	bufsize		= bsize;
	bufnum		= bnum;
	packetsize	= psize;

	totalbufsize = (size_t)bufsize * bufnum;

	datsize		= 0;
	bufcount	= 0;
	datpos		= 0;
	exdat		= 0;

	totalwrite	= 0;
	writepoint	= 0;

	bLock = FALSE;

	buftop = (unsigned char*)_aligned_malloc((size_t)bufsize * (bufnum + 1), (size_t)bufsize);
//	buftop = (unsigned char*)VirtualAlloc(NULL, (size_t)bufsize * (bufnum + 1), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	if(buftop == NULL){
		_tprintf(_T("書き込みバッファメモリの確保に失敗しました.\n"));
		exit(1);
	}

}

TSFileWrite::~TSFileWrite()
{
	CloseHandle(hWriteFile);

	_aligned_free(buftop);
//	VirtualFree(buftop, 0, MEM_RELEASE);

}

unsigned char* TSFileWrite::write()
{
	BOOL			bResult;

	if( !bLock && (datsize >= (size_t)bufsize) ) {

		DWORD			numwrite;

		while(datsize >= (size_t)bufsize) {

			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			datsize -= bufsize;

			bufcount++;
			if(bufcount == bufnum) bufcount = 0;
		}
	}

	size_t	cdatpos = datpos;

	datpos += packetsize;

	if(datpos >= totalbufsize) {
		exdat = (unsigned int)(datpos - totalbufsize);						// バッファの後ろにはみ出たデータの量
		datpos = exdat;
	}

	datsize += packetsize;
	writepoint += packetsize;

	if(datsize > totalbufsize) {
		_tprintf(_T("データ書き込み用バッファメモリがオーバーフローしました.\n変換を中止します.\n"));
		exit(1);
	}

	return	buftop + cdatpos;
}

void TSFileWrite::lock()
{
	bLock = TRUE;

	return;
}

void TSFileWrite::unlock()
{
	bLock = FALSE;

	return;
}

void TSFileWrite::release()
{
	bLock = FALSE;

	BOOL		bResult;
	DWORD		numwrite;

	while(datsize >= (size_t)bufsize) {

		if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

		bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

		if(!bResult){
			_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += numwrite;
		datsize -= bufsize;

		bufcount++;
		if(bufcount == bufnum) bufcount = 0;
	}

	return;
}

void TSFileWrite::finish()
{
	BOOL		bResult;
	DWORD		numwrite;

	while(datsize >= (size_t)bufsize) {

		if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

		bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

		if(!bResult){
			_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += numwrite;
		datsize -= bufsize;

		bufcount++;
		if(bufcount == bufnum) bufcount = 0;
	}

	if(bNoBuf)
	{
		if(datsize != 0) {
			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			CloseHandle(hWriteFile);
			hWriteFile = hWriteFileSub;

			int		back_H = -1;
			int		back_L = (int)datsize - (int)bufsize; 

			SetFilePointer(hWriteFile, back_L, (PLONG)&back_H, FILE_END);
			SetEndOfFile(hWriteFile);
			totalwrite += back_L;
		}
		else
		{
			CloseHandle(hWriteFile);
			hWriteFile = hWriteFileSub;
		}
	}
	else
	{
		if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

		bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, (DWORD)datsize, &numwrite, NULL);

		if(!bResult){
			_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += numwrite;
	}

	return;
}


//
// TSファイル書き込みクラス
// TSFileWriteMultipleLockのメンバ定義

TSFileWriteMultipleLock::TSFileWriteMultipleLock(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hWriteFile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);

		if(hWriteFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("保存先ファイルをFILE_FLAG_NO_BUFFERINGで作成するのに失敗しました. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hWriteFile = CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("保存先ファイルを作成するのに失敗しました. error code %d.\n"), GetLastError());
			_tprintf(_T("保存先ファイルを作成するのに失敗しました.\n"));
			exit(1);
		}
	}
	else
	{
		hWriteFileSub = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFileSub == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("保存先ファイルを開くのに失敗しました. error code %d.\n"), GetLastError());
			_tprintf(_T("保存先ファイルを開くのに失敗しました.\n"));
			exit(1);
		}
	}

	bufsize		= bsize;
	bufnum		= bnum;
	packetsize	= psize;

	totalbufsize = (size_t)bufsize * bufnum;

	datsize		= 0;
	bufcount	= 0;
	datpos		= 0;
	exdat		= 0;

	totalwrite	= 0;
	writepoint	= 0;

	buftop = (unsigned char*)_aligned_malloc((size_t)bufsize * (bufnum + 1), (size_t)bufsize);
//	buftop = (unsigned char*)VirtualAlloc(NULL, (size_t)bufsize * (bufnum + 1), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	if(buftop == NULL){
		_tprintf(_T("書き込みバッファメモリの確保に失敗しました.\n"));
		exit(1);
	}

	nLockCount = 0;

}

TSFileWriteMultipleLock::~TSFileWriteMultipleLock()
{
	CloseHandle(hWriteFile);

	_aligned_free(buftop);
//	VirtualFree(buftop, 0, MEM_RELEASE);

}

unsigned char* TSFileWriteMultipleLock::write()
{
	int				i;

	BOOL			bResult;
	DWORD			numwrite;

	if(nLockCount == 0) {

		while(datsize >= (size_t)bufsize) {

			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			datsize -= bufsize;

			bufcount++;
			if(bufcount == bufnum) bufcount = 0;
		}

	} else {

		while(lockDataSize[0] >= (size_t)bufsize) {

			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			datsize -= bufsize;
			for(i = 0; i < nLockCount; i++) lockDataSize[i] -= bufsize;

			bufcount++;
			if(bufcount == bufnum) bufcount = 0;
		}
	}

	size_t	cdatpos = datpos;

	datpos += packetsize;

	if(datpos >= totalbufsize) {
		exdat = (unsigned int)(datpos - totalbufsize);
		datpos = exdat;
	}

	datsize += packetsize;
	writepoint += packetsize;

	if(datsize > totalbufsize) {
		_tprintf(_T("データ書き込み用バッファメモリがオーバーフローしました.\n変換を中止します.\n"));
		exit(1);
	}

	return	buftop + cdatpos;
}

void TSFileWriteMultipleLock::lock(int uid)
{
	// uidを指定してロックする
	// 既にuidでロックされている場合は、その項目を除いて最後尾に追加し直す

	int			i;

	for(i = 0; i < nLockCount; i++) {
		if(lockUid[i] == uid) break;
	}

	if(i == nLockCount) {								// 指定したuidでは未ロックだった場合, あるいはnLockCountが0だった場合 

		nLockCount++;

		if(nLockCount > MAX_FILE_WRITE_LOCK_NUM) {
			_tprintf(_T("データ書き込み制御数の上限を超えました.変換を中止します.\n"));
			exit(1);
		}

		lockUid[i] = uid;
		lockDataSize[i] = datsize - packetsize;

		return;
	}


	// 指定したuidで既にロックされていた場合

	while(i < nLockCount - 1) {

		lockUid[i] = lockUid[i + 1];
		lockDataSize[i] = lockDataSize[i + 1];

		i++;
	}

	lockUid[i] = uid;
	lockDataSize[i] = datsize - packetsize;

	return;
}

void TSFileWriteMultipleLock::unlock(int uid)
{
	// uidを指定してロック解除する
	// 指定したuidで未ロックならそのまま戻る

	int			i;

	for(i = 0; i < nLockCount; i++) {
		if(lockUid[i] == uid) break;
	}

	if(i == nLockCount) return;						// 指定したuidでは未ロックだった場合, あるいはnLockCountが0だった場合 


	// 指定したuidでロックされていた場合

	while(i < nLockCount - 1) {

		lockUid[i] = lockUid[i + 1];
		lockDataSize[i] = lockDataSize[i + 1];

		i++;
	}

	nLockCount--;

	return;
}

void TSFileWriteMultipleLock::release(int uid)
{
	int				i;

	// 指定したuidでロック解除

	this->unlock(uid);

	
	// 未ロック部分をディスク出力する

	BOOL			bResult;
	DWORD			numwrite;

	if(nLockCount == 0) {

		while(datsize >= (size_t)bufsize) {

			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			datsize -= bufsize;

			bufcount++;
			if(bufcount == bufnum) bufcount = 0;
		}

	} else {

		while(lockDataSize[0] >= (size_t)bufsize) {

			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			datsize -= bufsize;
			for(i = 0; i < nLockCount; i++) lockDataSize[i] -= bufsize;

			bufcount++;
			if(bufcount == bufnum) bufcount = 0;
		}
	}


}

void TSFileWriteMultipleLock::finish()
{
	BOOL		bResult;
	DWORD		numwrite;

	while(datsize >= (size_t)bufsize) {

		if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

		bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

		if(!bResult){
			_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += numwrite;
		datsize -= bufsize;

		bufcount++;
		if(bufcount == bufnum) bufcount = 0;
	}

	if(bNoBuf)
	{
		if(datsize != 0) {
			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
			CloseHandle(hWriteFile);
			hWriteFile = hWriteFileSub;

			int		back_H = -1;
			int		back_L = (int)datsize - (int)bufsize; 

			SetFilePointer(hWriteFile, back_L, (PLONG)&back_H, FILE_END);
			SetEndOfFile(hWriteFile);
			totalwrite += back_L;
		}
		else
		{
			CloseHandle(hWriteFile);
			hWriteFile = hWriteFileSub;
		}
	}
	else
	{
		if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

		bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, (DWORD)datsize, &numwrite, NULL);

		if(!bResult){
			_tprintf(_T("データ書き込み中にエラーが発生しました. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += numwrite;
	}

	return;
}