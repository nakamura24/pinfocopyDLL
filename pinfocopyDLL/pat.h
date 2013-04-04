#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

// �萔

#define		MAX_PAT_SIZE					1288			// 7�p�P�b�g��

#define		PAT_NOT_PAT_PACKET				0
#define		PAT_NOT_STARTED					1
#define		PAT_STARTED						2
#define		PAT_CONTINUED					3
#define		PAT_SINGLE_PACKET_CRC_ERROR		4
#define		PAT_MULTI_PACKET_CRC_ERROR		5
#define		PAT_SINGLE_PACKET_CONVED		6
#define		PAT_MULTI_PACKET_CONVED			7
#define		PAT_LIFE_EXPIRED				8

#define		MAX_PAT_PACKET_LIFE				16000


// PAT�����N���X

class	PatProcess
{
public:
	int			pid;
	int			uid;

	PatProcess(const int numpid, const int tsid, const int serviceid, const int uniqueid);
// 	~PatProcess();

	int				getPacket(const unsigned char *packet, unsigned char *wbuf);

private:
	unsigned char	buf[MAX_PAT_SIZE];	// ������PAT�p�P�b�g����荞��ł���o�b�t�@
	int				buflen;				// ���̃f�[�^�̃g�[�^���̒���
	int				adaplen;			// �A�_�v�e�[�V�����t�B�[���h��
	int				seclen;				// �Z�N�V������

	unsigned char	*waddr[MAX_PAT_SIZE / 184];		// �ϊ��p�P�b�g�����o���ׂ̈̃A�h���X�L�^
	int				wcount;							// ���̌�

	unsigned char	pat[MAX_PAT_SIZE];	// ��s����PAT�̃o�b�t�@
	int				patlen;				// ���̃f�[�^�̃g�[�^���̒���

	unsigned char	conv[MAX_PAT_SIZE];	// ��s����PAT�̕ϊ���o�b�t�@
	int				convlen;			// ���̃f�[�^�̃g�[�^���̒���
	int				patnum;				// ���̃f�[�^�����p�P�b�g����
	int				patcount;			// �o�͎��̃J�E���g�p

	BOOL			bPatStarted;		// �����p�P�b�g�ɂ܂�����PAT���J�n������TRUE�ɂȂ�
	int				lifecount;			// �����p�P�b�g��PAT���J�n���Ă���̃p�P�b�g�J�E���g�p
										// �J�E���g��MAX_PAT_PACKET_LIFE�𒴂����炻��PAT�͖����ɂ���(overflow�h�~�p)

	int				ts_id;				// ���������p transport_stream_ID
	int				service_id;			// ���������p service_ID (program_number)

	BOOL			isCrcOk();			// buf��CRC�`�F�b�N
	BOOL			isNewPat();			// buf��pat�̔�r
	void			copyPat();			// buf����pat, conv�ւ̃R�s�[
	void			convPat();			// conv�ɃR�s�[����PAT�̕ϊ�
};