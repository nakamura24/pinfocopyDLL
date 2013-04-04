#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

#include "tsprocess.h"


//
// �p�P�b�g�����֌W���[�`��
//

int getPid(const unsigned char* buf)							// buf��0x47�w�b�_�̏ꏊ
{
	return (buf[0x01] & 0x1F) * 256 + buf[0x02];
}


int getPidValue(const unsigned char* buf)
{
	return (buf[0x00] & 0x1F) * 256 + buf[0x01];
}


BOOL isPsiTop(const unsigned char* buf)
{
	if( (buf[0x01] & 0x40) != 0 ) return TRUE;

	return FALSE;
}


BOOL isScrambled(const unsigned char* buf)
{
	if( (buf[0x03] & 0xC0) == 0 ) return FALSE;

	return TRUE;
}


int getAdapFieldLength(const unsigned char* buf)
{
	if( (buf[0x03] & 0x20) == 0) return 0;

	return buf[0x04] + 1;
}


int	getPointerFieldLength(const unsigned char* buf)
{
	if(!isPsiTop(buf)) return 0;

	return buf[0x04 + getAdapFieldLength(buf)];
}


int getSectionLength(const unsigned char* buf)						// buf��TableID�̏ꏊ
{
	return (buf[0x01] & 0x0F) * 256 + buf[0x02];
}


int getLength(const unsigned char* buf)
{
	return (buf[0x00] & 0x0F) * 256 + buf[0x01];
}


int getPsiLength(const unsigned char* buf)
{
	return (buf[0x06 + getAdapFieldLength(buf) + getPointerFieldLength(buf)] & 0x0F) * 256 + buf[0x07 + getAdapFieldLength(buf) + getPointerFieldLength(buf)];
}


int getPsiPacket(TSFileRead *tsin, unsigned char *psibuf, const int pid, const unsigned int packetsize)
{
	unsigned char	*buf;

	BOOL			bPsiFound		= FALSE;
	BOOL			bPsiStarted		= FALSE;
	int				psilen			= 0;
	int				section_length	= 0;

	while(1)
	{
		buf = tsin->read();
		if(buf == NULL) break;
		buf += (packetsize - 188);

		if( getPid(buf) == pid )
		{
			BOOL	bPsiTop = isPsiTop(buf);
			int		adaplen = getAdapFieldLength(buf);
			int		pflen	= getPointerFieldLength(buf);						// !bPsiTop�ȏꍇ�͖��Ӗ��Ȓl

			int		len;
			BOOL	bTop = FALSE;
			BOOL	bFirstSection = TRUE;
			int		i = adaplen + 4;

			while(i < 188)														// 188�o�C�g�p�P�b�g���ł̊e�Z�N�V�����̃��[�v
			{
				if(!bPsiTop) {
					len = 188 - i;
					bTop = FALSE;
				} else if( bFirstSection && (pflen != 0) ) {
					i++;
					len = pflen;
					bTop = FALSE;
					bFirstSection = FALSE;
				} else {
					if( bFirstSection && (pflen == 0) ) {
						i++;
						bFirstSection = FALSE;
					}
					if(buf[i] == 0xFF) break;									// TableID�̂���ׂ��ꏊ��0xFF(stuffing byte)�Ȃ炻�̃p�P�b�g�Ɋւ��鏈���͏I��
					len = getSectionLength(buf + i) + 3;						// �Z�N�V�����w�b�_�̓p�P�b�g�ɂ܂������Ĕz�u����邱�Ƃ͖����͂��Ȃ̂Ńp�P�b�g�͈͊O(188�o�C�g�ȍ~)��ǂ݂ɍs�����Ƃ͖����͂�
					if(i + len > 188) len = 188 - i;
					bTop = TRUE;
				}

				if(bPsiStarted && !bTop) {
					memcpy(psibuf + psilen, buf + i, len);
					psilen += len;
				}
				if(bTop) {
					memcpy(psibuf, buf + i, len);
					psilen = len;
					section_length = getSectionLength(psibuf);
					bPsiStarted = TRUE;
				}
				if( bPsiStarted && (psilen >= section_length + 3) ) {
					int		crc = calc_crc32(psibuf, section_length + 3);
					if(crc == 0) {
						bPsiFound = TRUE;
						break;
					}
					bPsiStarted = FALSE;
				}

				i += len;
			}

		}

		if(bPsiFound) break;
	}

	if(bPsiFound) return section_length + 3;

	return	0;
}


int parsePat(const unsigned char *buf, int *pmtpid)
{
	int		programcount = 0;

	int		seclen = getSectionLength(buf);
	int		i = 0x08;

	while( i < (seclen - 1) )
	{
		int		service_id = buf[i] * 256 + buf[i + 1];
		int		pid = getPidValue(buf + i + 2);

		if(service_id != 0) {								// service_id == 0 �� NIT
			
			int		j;
			BOOL	bNewPmtPid = TRUE;

			for(j = 0; j < programcount; j++) {
				if(pid == pmtpid[j]) bNewPmtPid = FALSE;
			}

			if(bNewPmtPid) pmtpid[programcount++] = pid;
		}

		i += 4;
	}

	return	programcount;
}


void parsePmt(const unsigned char *buf, int *pcrpid, int *videopid, int *rmpid, int *rmnum, const BOOL bRmCap, const BOOL bRmDat)
{
	int		seclen = getSectionLength(buf);
	int		pinfolen = getLength(buf + 0x0a);

	if(pcrpid != NULL) *pcrpid = getPidValue(buf + 0x08);

	// secondloop���珈��

	int		i = 0x0C + pinfolen;

	int		esinfolen;
	int		rmcount = 0;

	while( i < (seclen - 1) ) {

		// ������buf[i]��stream_type, 0x06(����,�����X�[�p�[), 0x0D(�f�[�^����), 0x02(mpeg2 video)

		esinfolen = getLength(buf + i + 0x03);

		if( bRmCap && (buf[i] == 0x06) ) rmpid[rmcount++] = getPidValue(buf + i + 0x01);
		if( bRmDat && (buf[i] == 0x0D) ) rmpid[rmcount++] = getPidValue(buf + i + 0x01);
		if( (buf[i] == 0x02) && (videopid != NULL) ) *videopid = getPidValue(buf + i + 0x01);

		i += (5 + esinfolen);
	}

	if(bRmCap || bRmDat) {
		rmpid[rmcount++] = 0x1FFF;															// NULL�p�P�b�gPID�ǉ�
		qsort(rmpid, rmcount, sizeof(int), compareForPidTable);								// �폜�pPID�̏����\�[�g qsort�ɕύX����
	}

	if(rmnum != NULL) *rmnum = rmcount;

	return;
}


int compareForPidTable(const void *item1, const void *item2)								// �폜PID�e�[�u�� qsort, bsearch �p�֐�
{
	return *(int*)item1 - *(int*)item2;
}


unsigned int calc_crc32(const unsigned char *buf, const int len)
{
	static unsigned int crc_table[] ={
		0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005, 
		0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD, 
		0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75, 
		0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD, 
		0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5, 
		0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D, 
		0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95, 
		0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D, 
		0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072, 
		0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA, 
		0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02, 
		0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA, 
		0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692, 
		0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A, 
		0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2, 
		0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A, 
		0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB, 
		0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53, 
		0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B, 
		0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623, 
		0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B, 
		0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3, 
		0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 
		0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3, 
		0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C, 
		0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24, 
		0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC, 
		0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654, 
		0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C, 
		0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4, 
		0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C, 
		0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
	};

	unsigned int		crc = 0xFFFFFFFFL;
	const unsigned char	*mybuf = buf;

	for(int	i = 0; i < len; i++) {
		crc = (crc << 8) ^ crc_table[ (crc >> 24) ^ *mybuf ];
		mybuf++;
	}

	return crc;
}


BOOL isPcrData(const unsigned char *buf)
{
	// buf��0x47�̈ʒu
	// adaptation_field�����݂��āA���̒�����0�łȂ��APCR_flag��1�Ȃ�

	if( ((buf[0x03] & 0x20) != 0) && (buf[0x04] != 0) && ( (buf[0x05] & 0x10) != 0) ) return TRUE;

	return FALSE;
}


__int64 getPcrValue(const unsigned char *buf)
{
	// buf��0x47�̈ʒu

	const unsigned char	*pcr = buf + 6;				// pcr_clock_reference_base�̐擪

	__int64		pcr_base = ((__int64)pcr[0x00] << 25) + ((__int64)pcr[0x01] << 17) + ((__int64)pcr[0x02] << 9) + ((__int64)pcr[0x03] << 1) + (((__int64)pcr[0x04] & 0x80) >> 7);
	__int64		pcr_ext  = (((__int64)pcr[0x04] & 0x01) << 8) + (__int64)pcr[0x05];

	__int64		pcr_value  = pcr_base * 300 + pcr_ext;

	return pcr_value;
}


void initTsFileRead(TsReadProcess *ts, HANDLE hFile, const int packetsize)
{
	ts->hFile		= hFile;
	ts->psize		= packetsize;
	ts->poffset		= packetsize - 188;
	ts->datasize	= 0;
	ts->pos			= 0;
	ts->buf			= ts->databuf + READBUFMERGIN;
	ts->bShowError	= FALSE;

	memset(ts->databuf, 0, sizeof(unsigned char) * (READBUFSIZE + READBUFMERGIN));

	return;
}


void setPointerTsFileRead(TsReadProcess *ts, const __int64 pointer)
{
	LARGE_INTEGER	fp;

	ts->pos = (int)(pointer % READBUFSIZE);
	fp.QuadPart = pointer - ts->pos;
	SetFilePointerEx(ts->hFile, fp, NULL, FILE_BEGIN);

	DWORD	dwNumRead;
	ReadFile(ts->hFile, ts->buf, READBUFSIZE, &dwNumRead, NULL);

	ts->datasize = dwNumRead;

	return;
}


void setPosTsFileRead(TsReadProcess *ts, const int startpos)
{
	LARGE_INTEGER	fsize;
	GetFileSizeEx(ts->hFile, &fsize);

	__int64		fp = fsize.QuadPart	* startpos / 100;
	setPointerTsFileRead(ts, fp);

	return;
}


void showErrorTsFileRead(TsReadProcess *ts, const BOOL bShowError)
{
	ts->bShowError = bShowError;

	return;
}


unsigned char* getPacketTsFileRead(TsReadProcess *ts)
{
	DWORD	dwNumRead;

	int		remainings = ts->datasize - ts->pos;

	if(remainings < ts->psize) {														// �o�b�t�@���̃f�[�^�c�肪packetsize�����ɂȂ����ꍇ, �����̃f�[�^�ǂݍ��݂����݂�

		if(ts->datasize != READBUFSIZE) return NULL;									// ���Ƀt�@�C���I�[�܂œǂݏI����Ă���ꍇ,  �I���T�C��(NULL)��Ԃ�

		memcpy(ts->buf - remainings, ts->buf + ts->pos, remainings);					// �f�[�^���[�����o�b�t�@�O�ɃR�s�[
		ts->pos	= 0 - remainings;

		ReadFile(ts->hFile, ts->buf, READBUFSIZE, &dwNumRead, NULL);					// �o�b�t�@�Ƀf�[�^�ǂݍ���
		ts->datasize = dwNumRead;

		if( (ts->datasize - ts->pos) < ts->psize ) return NULL;							// ������ǂ�ł�packetsize�ɑ���Ȃ��ꍇ�͏I��
	}

	if(ts->buf[ts->pos + ts->poffset] == 0x47) {
	
		unsigned char	*packetadr = ts->buf + ts->pos;
		ts->pos += ts->psize;

		return packetadr;																// �p�P�b�g�w�b�_0x47���m�F�����炻�̃A�h���X��Ԃ�
	}

	// �p�P�b�g�w�b�_0x47�����������ꍇ�A�擪��T��

//	_tprintf(_T("sync lost.\n"));

	int			resyncsize = ts->psize * 2 + 1 + ts->poffset;							// 3�̘A������w�b�_0x47���m�F������ē��������ƌ��Ȃ��̂ŁA����ɕK�v�ȃT�C�Y
	__int64		brokenbyte = 0;

	while(1)
	{
		remainings = ts->datasize - ts->pos;

		if(remainings < resyncsize) {													// �c��f�[�^���ē����m�F�ɕK�v�ȃT�C�Y��菭�Ȃ��ꍇ

			if(ts->datasize != READBUFSIZE) return NULL;								// ���Ƀt�@�C���I�[�܂œǂݏI����Ă���ꍇ,  �I���T�C��(NULL)��Ԃ�

			memcpy(ts->buf - remainings, ts->buf + ts->pos, remainings);				// �f�[�^���[�����o�b�t�@�O�ɃR�s�[
			ts->pos	= 0 - remainings;

			ReadFile(ts->hFile, ts->buf, READBUFSIZE, &dwNumRead, NULL);				// �o�b�t�@�Ƀf�[�^�ǂݍ���
			ts->datasize = dwNumRead;

			if( (ts->datasize - ts->pos) < resyncsize ) return NULL;					// ������ǂ�ł�resyncsize�ɑ���Ȃ��ꍇ�͏I��
		}

		if( (ts->buf[ts->pos + ts->poffset] == 0x47) && (ts->buf[ts->pos + ts->psize + ts->poffset] == 0x47) && (ts->buf[ts->pos + ts->psize * 2 + ts->poffset] == 0x47) ) break;	// �p�P�b�g�w�b�_�����o����

		ts->pos++;
		brokenbyte++;
	}

	// 0x47 ������

	if(ts->bShowError) _tprintf(_T("%I64d�o�C�g�̕s���ȃf�[�^���폜���܂���.\n"), brokenbyte);

	unsigned char	*packetadr = ts->buf + ts->pos;
	ts->pos += ts->psize;

	return packetadr;																	// �p�P�b�g�w�b�_0x47�̃A�h���X��Ԃ�
}


