#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

// �萔

#define		MAX_PMT_SIZE					1288			// 7�p�P�b�g��

#define		PMT_NOT_PMT_PACKET				0
#define		PMT_NOT_STARTED					1
#define		PMT_STARTED						2
#define		PMT_CONTINUED					3
#define		PMT_SINGLE_PACKET_CRC_ERROR		4
#define		PMT_MULTI_PACKET_CRC_ERROR		5
#define		PMT_SINGLE_PACKET_CONVED		6
#define		PMT_MULTI_PACKET_CONVED			7
#define		PMT_LIFE_EXPIRED				8

#define		MAX_PMT_PACKET_LIFE				16000


// PMT�����N���X

class	PmtProcess
{
public:
	int			pid;
	int			uid;

	PmtProcess(const int numpid, const int serviceid, const int uniqueid);
// 	~PmtProcess();

	int				getPacket(const unsigned char *packet, unsigned char *wbuf);

private:
	unsigned char	buf[MAX_PMT_SIZE];	// ������PMT�p�P�b�g����荞��ł���o�b�t�@
	int				buflen;				// ���̃f�[�^�̃g�[�^���̒���
	int				adaplen;			// �A�_�v�e�[�V�����t�B�[���h��
	int				seclen;				// �Z�N�V������

	unsigned char	*waddr[MAX_PMT_SIZE / 184];		// �ϊ��p�P�b�g�����o���ׂ̈̃A�h���X�L�^
	int				wcount;							// ���̌�

	unsigned char	pmt[MAX_PMT_SIZE];	// ��s����PMT�̃o�b�t�@
	int				pmtlen;				// ���̃f�[�^�̃g�[�^���̒���

	unsigned char	conv[MAX_PMT_SIZE];	// ��s����PMT�̕ϊ���o�b�t�@
	int				convlen;			// ���̃f�[�^�̃g�[�^���̒���
	int				pmtnum;				// ���̃f�[�^�����p�P�b�g����
	int				pmtcount;			// �o�͎��̃J�E���g�p

	BOOL			bPmtStarted;		// �����p�P�b�g�ɂ܂�����PMT���J�n������TRUE�ɂȂ�
	int				lifecount;			// �����p�P�b�g��PMT���J�n���Ă���̃p�P�b�g�J�E���g�p
										// �J�E���g��MAX_PMT_PACKET_LIFE�𒴂����炻��PMT�͖����ɂ���(overflow�h�~�p)

	int				service_id;			// ���������p service_ID (program_number)

	BOOL			isCrcOk();			// buf��CRC�`�F�b�N
	BOOL			isNewPmt();			// buf��pmt�̔�r
	void			copyPmt();			// buf����pmt, conv�ւ̃R�s�[
	void			convPmt();			// conv�ɃR�s�[����PMT�̕ϊ�
};