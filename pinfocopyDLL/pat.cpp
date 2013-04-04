#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

#include "pat.h"
#include "tsprocess.h"

//
//
// PAT�����N���X
// PatProcess�̃����o��`

PatProcess::PatProcess(const int numpid, const int tsid, const int serviceid, const int uniqueid)
{
	pid = numpid;
	uid = uniqueid;

	ts_id		= tsid;
	service_id	= serviceid;

	bPatStarted = FALSE;

	memset(buf, 0xFF, MAX_PAT_SIZE);
	memset(pat, 0xFF, MAX_PAT_SIZE);
	memset(conv, 0xFF, MAX_PAT_SIZE);

	buflen = 0;
	adaplen = 0;
	seclen = 0;

	patlen = 0;
	convlen = 0;

	wcount = 0;

	patnum = 0;
	patcount = 0;

	lifecount = 0;
}

// PatProcess::~PatProcess()
// {
// }

int PatProcess::getPacket(const unsigned char *packet, unsigned char *wbuf)
{

	if(getPid(packet) != pid) {											// PAT�p�P�b�g�ł͖��������ꍇ

		if(!bPatStarted) return PAT_NOT_PAT_PACKET;

		lifecount++;

		if(lifecount > MAX_PAT_PACKET_LIFE) {
			bPatStarted = FALSE;
			lifecount = 0;
			return PAT_LIFE_EXPIRED;
		}

		return PAT_NOT_PAT_PACKET;
	}


	// �󂯎����PAT�p�P�b�g������

	if(isPsiTop(packet))												// PAT�p�P�b�g�擪�̏ꍇ
	{
		wcount = 0;
		waddr[wcount++] = wbuf;

		memcpy(buf, packet + 4, 184);
		buflen = 184;

		adaplen = getAdapFieldLength(packet);
		seclen  = getSectionLength(packet + adaplen + 5);

		bPatStarted = TRUE;
//		lifecount = 0;

		if( buflen < (adaplen + seclen + 4) ) return PAT_STARTED;		// ���̃p�P�b�g�ɂ܂�����������Ƃ�
	}
	else																// PAT�p�P�b�g��擪�̏ꍇ
	{
		if(!bPatStarted) return PAT_NOT_STARTED;						// PAT��START���Ă��Ȃ��̂ɔ�擪��������

		waddr[wcount++] = wbuf;
		memcpy(buf + buflen, packet + 4, 184);

		buflen += 184;

		if( buflen < (adaplen + seclen + 4) ) return PAT_CONTINUED;		// ���̃p�P�b�g�ɂ܂�����������Ƃ�
	}


	// �p�P�b�g����������CRC�`�F�b�N

	if( !(this->isCrcOk()) ) {											// CRC������Ȃ��ꍇ

		bPatStarted = FALSE;
		lifecount = 0;

		if(wcount == 1) {
			return PAT_SINGLE_PACKET_CRC_ERROR;
		} else {
			return PAT_MULTI_PACKET_CRC_ERROR;
		}
	}


	// ��s����PAT�Ɣ�r, �V����PAT�Ȃ����ւ�

	if(this->isNewPat()) {
//		printf("new pat found. %.2X\n", (buf[adaplen + 0x06] & 0x3E) >> 1);
		this->copyPat();
		this->convPat();
	}


	// �ϊ��ς�PAT���o��

	int		i;

	for(i = 0; i < wcount; i++) {

		memcpy(waddr[i] + 4, conv + 184 * patcount, 184);

		if(patcount == 0) {
			*(waddr[i] + 1) |= 0x40;									// payload_unit_start_indicator���Z�b�g����
		} else {
			*(waddr[i] + 1) &= 0xBF;									// payload_unit_start_indicator�����Z�b�g����
		}

		patcount++;
		if(patcount == patnum) patcount = 0;
	}

	bPatStarted = FALSE;
	lifecount = 0;

	if(wcount == 1) return PAT_SINGLE_PACKET_CONVED;

	return PAT_MULTI_PACKET_CONVED;
}

BOOL PatProcess::isCrcOk()
{
	unsigned int	crc = calc_crc32(buf + adaplen + 1, seclen + 3);

	if(crc == 0) return TRUE;

	return FALSE;
}

BOOL PatProcess::isNewPat()
{
	if( patlen != (adaplen + seclen + 4) ) return TRUE;

	int result = memcmp(pat, buf, patlen);

	if(result == 0) return FALSE;

	return TRUE;
}

void PatProcess::copyPat()
{
	patlen = adaplen + seclen + 4;
	memcpy(pat, buf, patlen);
	memcpy(conv, buf, patlen);

	return;
}

void PatProcess::convPat()
{
	unsigned char	*dat = conv + adaplen;

	int		section_length = getSectionLength(dat + 1);

	dat[0x04] = ts_id >> 8;											// transport_stream_ID ��������
	dat[0x05] = ts_id & 0x00FF;

	int		i = 0x09;

	while(i < section_length)
	{
		int		program_number = dat[i] * 256 + dat[i + 1];
		
		if(program_number != 0) {
			dat[i + 0] = service_id >> 8;							// serviceID (program_number) ��������
			dat[i + 1] = service_id & 0x00FF;
			break;
		}

		i += 4;
	}


	// CRC�Čv�Z

	unsigned int	crc = calc_crc32(dat + 1, section_length - 1);

	dat[section_length + 0] = (crc >> 24) & 0xFF;
	dat[section_length + 1] = (crc >> 16) & 0xFF;
	dat[section_length + 2] = (crc >>  8) & 0xFF;
	dat[section_length + 3] = (crc >>  0) & 0xFF;

	convlen = section_length + adaplen + 4;							// conv�S�̂̃f�[�^��

	memset(conv + convlen, 0xFF, MAX_PAT_SIZE - convlen);			// �f�[�^�̌���0xFF�Ŗ��߂�

	patnum = convlen / 184;											// �f�[�^�����p�P�b�g�����v�Z
	if( (convlen % 184) != 0 ) patnum++;

	patcount = 0;													// �o�͗p�J�E���g��0�ɖ߂�

	return;
}