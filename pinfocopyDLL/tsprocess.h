#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "tsinout.h"

// �萔�Ȃ�

#define			READBUFSIZE				65536
#define			READBUFMERGIN			1024


// �f�B�X�N���o�͗p

typedef struct {
	HANDLE			hFile;
	int				psize;
	int				poffset;
	unsigned char	databuf[READBUFMERGIN + READBUFSIZE];
	unsigned char	*buf;
	int				datasize;
	int				pos;
	BOOL			bShowError;
} TsReadProcess;


// �v���g�^�C�v�錾

int				getPid(const unsigned char*);
int				getPidValue(const unsigned char*);
BOOL			isPsiTop(const unsigned char*);
BOOL			isScrambled(const unsigned char*);
int				getAdapFieldLength(const unsigned char*);
int				getPointerFieldLength(const unsigned char*);
int				getSectionLength(const unsigned char*);
int				getLength(const unsigned char*);
int				getPsiLength(const unsigned char*);
int				getPsiPacket(TSFileRead*, unsigned char*, const int, const unsigned int);
int				parsePat(const unsigned char*, int*);
void			parsePmt(const unsigned char*, int*, int*, int*, int*, const BOOL, const BOOL);
int				compareForPidTable(const void*, const void*);
unsigned int	calc_crc32(const unsigned char*, const int);
BOOL			isPcrData(const unsigned char*);
__int64			getPcrValue(const unsigned char*);

void			initTsFileRead(TsReadProcess*, HANDLE, const int);
void			setPointerTsFileRead(TsReadProcess*, const __int64);
void			setPosTsFileRead(TsReadProcess*, const int);
void			showErrorTsFileRead(TsReadProcess*, const BOOL);
unsigned char*	getPacketTsFileRead(TsReadProcess*);

