#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

#include "tsinout.h"


//
//
// TS�t�@�C���ǂݍ��݃N���X 
// TSFileRead�̃����o��`

TSFileRead::TSFileRead(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hReadFile = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

		if(hReadFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("�ǂݍ��݃t�@�C����FILE_FLAG_NO_BUFFERING�ŊJ���̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hReadFile = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hReadFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("�ǂݍ��݃t�@�C�����J���̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
			_tprintf(_T("�ǂݍ��݃t�@�C�����J���̂Ɏ��s���܂���.\n"));
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

	filepos = -1;		// �܂��f�[�^����

	buftop = (unsigned char*)_aligned_malloc((size_t)bufsize * (bufnum + 1), (size_t)bufsize);
//	buftop = (unsigned char*)VirtualAlloc(NULL, (size_t)bufsize * (bufnum + 1), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	if(buftop == NULL){
		_tprintf(_T("�ǂݍ��݃o�b�t�@�������̊m�ۂɎ��s���܂���.\n"));
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
		_tprintf(_T("�f�[�^�ǂݍ��ݒ��ɃG���[���������܂���.(�f�[�^�͈͊O�̓ǂݍ���)\n"));
		exit(1);
	}

	if( (filepos == -1) || (filepos > fpoint) || ( (filepos + (__int64)bufsize * (__int64)bufnum) <= fpoint) )
	{
		// �܂��o�b�t�@�Ƀf�[�^�𖢓ǂ̏ꍇ�A�o�b�t�@�ɂ���f�[�^�͈͊O��seek�����ꍇ

		filepos = fpoint - (fpoint % bufsize);

		LARGE_INTEGER	fbase;
		fbase.QuadPart = filepos;
		SetFilePointerEx(hReadFile, fbase, NULL, FILE_BEGIN);

		datpoint = (fpoint % bufsize) + bufsize;

		bFileEnd  = FALSE;
		datsize   = bufsize;		// bufsize���̃Q�^
		totalread = 0;
		readpoint = fpoint;

		unsigned int		i;
		DWORD				numread;

		for(i = 1; i <= bufnum; i++) {
			bResult = ReadFile(hReadFile, buftop + (size_t)i * bufsize, bufsize, &numread, NULL);

			if(!bResult){
				_tprintf(_T("�f�[�^�ǂݍ��ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
		// �o�b�t�@�ɑ��݂���f�[�^�͈͓���seek����ꍇ

		datpoint = (size_t)(fpoint - filepos + bufsize);

		totalread = 0;
		readpoint = fpoint;
	}

}

unsigned char* TSFileRead::read()
{
	BOOL		bResult;

	size_t		remain = datsize - datpoint;

	if( remain < packetsize ) {							// �o�b�t�@���̃f�[�^�c�肪packetsize�����ɂȂ�����

		if(bFileEnd) {									// ���Ƀt�@�C���I�[�܂œǂݏI����Ă���ꍇ
			totalread += remain;
			readpoint += remain;
			return	NULL;								// �I���T�C����Ԃ�
		}

		memcpy(buftop + bufsize - remain, buftop + datpoint, remain);					// �f�[�^���[�����o�b�t�@�O�ɃR�s�[
		datpoint = bufsize - remain;
		datsize  = bufsize;

		unsigned int		i;
		DWORD				numread;

		filepos += ( (__int64)bufsize * bufnum);

		for(i = 1; i <= bufnum; i++) {
			bResult = ReadFile(hReadFile, buftop + (size_t)i * bufsize, bufsize, &numread, NULL);			// �c����t�@�C������ǂݍ���

			if(!bResult){
				_tprintf(_T("�f�[�^�ǂݍ��ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
				exit(1);
			}

			datsize += numread;
			if(numread != bufsize) {
				bFileEnd = TRUE;
				break;
			}
		}

		if( (datsize - datpoint) < packetsize ) {										// �c��̃f�[�^��ǂݍ���ł�packetsize��������������
			totalread += (datsize - datpoint);
			readpoint += (datsize - datpoint);
			return	NULL;																// �I���T�C����Ԃ�
		}
	}


	unsigned int	h_offset = packetsize - 188;

	if(buftop[datpoint + h_offset] == 0x47) {											// �p�P�b�g�w�b�_0x47���������������ꍇ
		size_t	cdatpoint = datpoint;
		datpoint  += packetsize;
		totalread += packetsize;
		readpoint += packetsize;
		return	buftop + cdatpoint;														// ���̃|�C���^��Ԃ�
	}


	// �p�P�b�g�w�b�_0x47�����������ꍇ�A�擪��T��

	unsigned int	resyncsize = packetsize * 2 + 1 + h_offset;

	__int64			removedat = 0;

//	_tprintf(_T("sync lost.\n"));

	while(1)
	{
		remain = datsize - datpoint;

		if(remain < resyncsize) {

			if(bFileEnd) {																	// ���Ƀt�@�C���I�[�܂œǂݏI����Ă���ꍇ
				totalread += remain;
				readpoint += remain;
				return	NULL;																// �I���T�C����Ԃ�
			}

			memcpy(buftop + bufsize - remain, buftop + datpoint, remain);					// �f�[�^���[�����o�b�t�@�O�ɃR�s�[
			datpoint = bufsize - remain;
			datsize  = bufsize;

			unsigned int		i;
			DWORD				numread;

			filepos += ( (__int64)bufsize * bufnum);

			for(i = 1; i <= bufnum; i++) {
				bResult = ReadFile(hReadFile, buftop + (size_t)i * bufsize, bufsize, &numread, NULL);			// �c����t�@�C������ǂݍ���

				if(!bResult){
					_tprintf(_T("�f�[�^�ǂݍ��ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
					exit(1);
				}

				datsize += numread;
				if(numread != bufsize) {
					bFileEnd = TRUE;
					break;
				}
			}

			if( (datsize - datpoint) < resyncsize) {										// �c��̃f�[�^��ǂݍ���ł�resyncsize�ȉ�����������
				totalread += (datsize - datpoint);
				readpoint += (datsize - datpoint);
				return	NULL;																// �I���T�C����Ԃ�
			}
		}

		if( (buftop[datpoint + h_offset] == 0x47) && (buftop[datpoint + packetsize + h_offset] == 0x47) && (buftop[datpoint + packetsize * 2 + h_offset] == 0x47) ) {
			break;		// 0x47 sync���񕜂�����break
		}

		datpoint++;
		removedat++;
		totalread++;
		readpoint++;
	}


	// 0x47 sync��

	if(bPacketError) _tprintf(_T("%I64d�o�C�g�̕s���ȃf�[�^���폜���܂���.\n"), removedat);

	size_t	cdatpoint = datpoint;
	datpoint  += packetsize;
	totalread += packetsize;
	readpoint += packetsize;

	return	buftop + cdatpoint;															// ���̃|�C���^��Ԃ�

}

void TSFileRead::showPacketError(BOOL bError)
{
	bPacketError = bError;

	return;
}


//
//
// TS�t�@�C���������݃N���X
// TSFileWriteAuto�̃����o��`

TSFileWriteAuto::TSFileWriteAuto(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hWriteFile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);

		if(hWriteFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("�ۑ���t�@�C����FILE_FLAG_NO_BUFFERING�ō쐬����̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hWriteFile = CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("�ۑ���t�@�C�����쐬����̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
			_tprintf(_T("�ۑ���t�@�C�����쐬����̂Ɏ��s���܂���.\n"));
			exit(1);
		}
	}
	else
	{
		hWriteFileSub = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFileSub == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("�ۑ���t�@�C�����J���̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
			_tprintf(_T("�ۑ���t�@�C�����J���̂Ɏ��s���܂���.\n"));
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
		_tprintf(_T("�������݃o�b�t�@�������̊m�ۂɎ��s���܂���.\n"));
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
				exit(1);
			}

			totalwrite += numwrite;
	}

	if(bNoBuf)
	{
		if(bmod != 0) {
			bResult = WriteFile(hWriteFile, buftop + (size_t)bcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
			_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += bmod;
	}

}


//
// TS�t�@�C���������݃N���X
// TSFileWrite�̃����o��`

TSFileWrite::TSFileWrite(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hWriteFile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);

		if(hWriteFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("�ۑ���t�@�C����FILE_FLAG_NO_BUFFERING�ō쐬����̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hWriteFile = CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("�ۑ���t�@�C�����쐬����̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
			_tprintf(_T("�ۑ���t�@�C�����쐬����̂Ɏ��s���܂���.\n"));
			exit(1);
		}
	}
	else
	{
		hWriteFileSub = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFileSub == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("�ۑ���t�@�C�����J���̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
			_tprintf(_T("�ۑ���t�@�C�����J���̂Ɏ��s���܂���.\n"));
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
		_tprintf(_T("�������݃o�b�t�@�������̊m�ۂɎ��s���܂���.\n"));
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
		exdat = (unsigned int)(datpos - totalbufsize);						// �o�b�t�@�̌��ɂ͂ݏo���f�[�^�̗�
		datpos = exdat;
	}

	datsize += packetsize;
	writepoint += packetsize;

	if(datsize > totalbufsize) {
		_tprintf(_T("�f�[�^�������ݗp�o�b�t�@���������I�[�o�[�t���[���܂���.\n�ϊ��𒆎~���܂�.\n"));
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
			_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
			_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
			_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += numwrite;
	}

	return;
}


//
// TS�t�@�C���������݃N���X
// TSFileWriteMultipleLock�̃����o��`

TSFileWriteMultipleLock::TSFileWriteMultipleLock(_TCHAR *fname, unsigned int bsize, unsigned int bnum, unsigned int psize, BOOL nobuf)
{
	bNoBuf = nobuf;

	if(bNoBuf)
	{
		hWriteFile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);

		if(hWriteFile == INVALID_HANDLE_VALUE) {
			bNoBuf = FALSE;
//			_tprintf(_T("�ۑ���t�@�C����FILE_FLAG_NO_BUFFERING�ō쐬����̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
		}
	}

	if(!bNoBuf)
	{	
		hWriteFile = CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFile == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("�ۑ���t�@�C�����쐬����̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
			_tprintf(_T("�ۑ���t�@�C�����쐬����̂Ɏ��s���܂���.\n"));
			exit(1);
		}
	}
	else
	{
		hWriteFileSub = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		
		if(hWriteFileSub == INVALID_HANDLE_VALUE) {
//			_tprintf(_T("�ۑ���t�@�C�����J���̂Ɏ��s���܂���. error code %d.\n"), GetLastError());
			_tprintf(_T("�ۑ���t�@�C�����J���̂Ɏ��s���܂���.\n"));
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
		_tprintf(_T("�������݃o�b�t�@�������̊m�ۂɎ��s���܂���.\n"));
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
		_tprintf(_T("�f�[�^�������ݗp�o�b�t�@���������I�[�o�[�t���[���܂���.\n�ϊ��𒆎~���܂�.\n"));
		exit(1);
	}

	return	buftop + cdatpos;
}

void TSFileWriteMultipleLock::lock(int uid)
{
	// uid���w�肵�ă��b�N����
	// ����uid�Ń��b�N����Ă���ꍇ�́A���̍��ڂ������čŌ���ɒǉ�������

	int			i;

	for(i = 0; i < nLockCount; i++) {
		if(lockUid[i] == uid) break;
	}

	if(i == nLockCount) {								// �w�肵��uid�ł͖����b�N�������ꍇ, ���邢��nLockCount��0�������ꍇ 

		nLockCount++;

		if(nLockCount > MAX_FILE_WRITE_LOCK_NUM) {
			_tprintf(_T("�f�[�^�������ݐ��䐔�̏���𒴂��܂���.�ϊ��𒆎~���܂�.\n"));
			exit(1);
		}

		lockUid[i] = uid;
		lockDataSize[i] = datsize - packetsize;

		return;
	}


	// �w�肵��uid�Ŋ��Ƀ��b�N����Ă����ꍇ

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
	// uid���w�肵�ă��b�N��������
	// �w�肵��uid�Ŗ����b�N�Ȃ炻�̂܂ܖ߂�

	int			i;

	for(i = 0; i < nLockCount; i++) {
		if(lockUid[i] == uid) break;
	}

	if(i == nLockCount) return;						// �w�肵��uid�ł͖����b�N�������ꍇ, ���邢��nLockCount��0�������ꍇ 


	// �w�肵��uid�Ń��b�N����Ă����ꍇ

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

	// �w�肵��uid�Ń��b�N����

	this->unlock(uid);

	
	// �����b�N�������f�B�X�N�o�͂���

	BOOL			bResult;
	DWORD			numwrite;

	if(nLockCount == 0) {

		while(datsize >= (size_t)bufsize) {

			if( (bufcount == 0) && (exdat != 0) ) memcpy(buftop, buftop + totalbufsize, exdat);

			bResult = WriteFile(hWriteFile, buftop + (size_t)bufcount * bufsize, bufsize, &numwrite, NULL);

			if(!bResult){
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
			_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
				_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
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
			_tprintf(_T("�f�[�^�������ݒ��ɃG���[���������܂���. error code %d.\n"), GetLastError());
			exit(1);
		}

		totalwrite += numwrite;
	}

	return;
}