#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

// �萔

#define			MAX_FILE_WRITE_LOCK_NUM			256


// TS�t�@�C���ǂݍ��݃N���X

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

	unsigned int	bufsize;			// �o�b�t�@1�ʂ̗e��
	unsigned int	bufnum;				// �o�b�t�@�̌�
	unsigned int	packetsize;			// �p�P�b�g�T�C�Y 188 or 192

	__int64			filepos;			// �o�b�t�@���̃f�[�^�擪�̃t�@�C���ɂ�����ʒu�Cseek()�Ŏg�p
	
	unsigned char	*buftop;			// �o�b�t�@�̐擪�A�h���X�i���ۂ͎��buftop + bufsize�̈ʒu����g�p����j
	size_t			datsize;			// ���o�b�t�@���ɂ���f�[�^�̗e�� + �o�b�t�@1�ʕ��̃Q�^
	size_t			datpoint;			// �o�b�t�@���̍��ǂ�ł�ꏊ + �o�b�t�@1�ʕ��̃Q�^

	BOOL			bFileEnd;			// �t�@�C���I�[�܂œǂ񂾂�TRUE
	BOOL			bNoBuf;				// no buffering�L��(FILE_FLAG_NO_BUFFERING)�Ȃ�TRUE
	BOOL			bPacketError;		// TRUE�Ȃ�"�s����...�폜���܂���."��\������
};



// TS�t�@�C�������o���N���X
// ���T�C�Y�Ŏ�����������

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
	HANDLE			hWriteFile;			// �������ݗp��n���h��
	HANDLE			hWriteFileSub;		// ��n���h����FILE_FLAG_NO_BUFFERING�̏ꍇ�̃T�u�n���h��

	unsigned int	bufsize;			// �o�b�t�@1�ʂ̗e��
	unsigned int	bufnum;				// �o�b�t�@�̌�
	size_t			totalbufsize;		// bufsize * bufnum;

	unsigned int	packetsize;			// �p�P�b�g�T�C�Y 188 or 192

	unsigned char	*buftop;			// �o�b�t�@�̐擪
	size_t			datsize;			// ���o�b�t�@���ɂ���f�[�^�̗e��

	BOOL			bNoBuf;				// no buffering�L��(FILE_FLAG_NO_BUFFERING)�Ȃ�TRUE

};


// TS�t�@�C�������o���N���X
// �������݃^�C�~���O����^(�V���O�����b�N)

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
	HANDLE			hWriteFile;			// �������ݗp��n���h��
	HANDLE			hWriteFileSub;		// ��n���h����FILE_FLAG_NO_BUFFERING�̏ꍇ�̃T�u�n���h��

	unsigned int	bufsize;			// �o�b�t�@1�ʂ̗e��
	unsigned int	bufnum;				// �o�b�t�@�̌�
	size_t			totalbufsize;		// bufsize * bufnum

	unsigned int	packetsize;			// �p�P�b�g�T�C�Y 188 or 192

	unsigned char	*buftop;			// �o�b�t�@�̐擪
	
	unsigned int	bufcount;			// ���f�[�^�̐擪������o�b�t�@�̔ԍ� 0�`(bufnum-1)
	size_t			datsize;			// ���o�b�t�@���ɂ���f�[�^�̗e��
	size_t			datpos;				// �o�b�t�@���̌��݂̏������݈ʒu
	unsigned int	exdat;				// �o�b�t�@�̍Ōォ��͂ݏo�����f�[�^�̗�	-> �o�b�t�@�擪�ɃR�s�[�����

	BOOL			bLock;				// �o�b�t�@����f�B�X�N�ւ̏����o�������b�N����Ă��邩�ǂ���
	BOOL			bNoBuf;				// no buffering�L��(FILE_FLAG_NO_BUFFERING)�Ȃ�TRUE

};


// TS�t�@�C�������o���N���X
// �}���`���b�N����^

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
	HANDLE			hWriteFile;				// �������ݗp��n���h��
	HANDLE			hWriteFileSub;			// ��n���h����FILE_FLAG_NO_BUFFERING�̏ꍇ�̃T�u�n���h��

	unsigned int	bufsize;				// �o�b�t�@1�ʂ̗e��
	unsigned int	bufnum;					// �o�b�t�@�̌�
	size_t			totalbufsize;			// bufsize * bufnum

	unsigned int	packetsize;				// �p�P�b�g�T�C�Y 188 or 192

	unsigned  char	*buftop;				// �o�b�t�@�̐擪
	
	unsigned int	bufcount;				// ���f�[�^�̐擪������o�b�t�@�̔ԍ� 0�`(bufnum-1)
	size_t			datsize;				// ���o�b�t�@���ɂ���f�[�^�̗e��
	size_t			datpos;					// �o�b�t�@���̌��݂̏������݈ʒu
	unsigned int	exdat;					// �o�b�t�@�̍Ōォ��͂ݏo�����f�[�^�̗�	-> �o�b�t�@�擪�ɃR�s�[�����

	int				nLockCount;								// �o�b�t�@����f�B�X�N�ւ̏����o�������b�N���Ă���uid�̐�
	int				lockUid[MAX_FILE_WRITE_LOCK_NUM];		// ���b�N���Ă���uid
	size_t			lockDataSize[MAX_FILE_WRITE_LOCK_NUM];	// ���b�N���Ă���T�C�Y

	BOOL			bNoBuf;					// no buffering�L��(FILE_FLAG_NO_BUFFERING)�Ȃ�TRUE
};
