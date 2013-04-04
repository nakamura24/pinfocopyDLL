// pinfocopy.cpp : �R���\�[�� �A�v���P�[�V�����̃G���g�� �|�C���g���`���܂��B
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <locale.h>

#include "tsinout.h"
#include "tsprocess.h"
#include "proginfo.h"
#include "convtounicode.h"
#include "sitprocess.h"
#include "pat.h"
#include "pmt.h"


// �萔�Ȃ�

#define		INBUFSIZE				(64 * 1024)
#define		INBUFNUM				1
#define		OUTBUFSIZE				(64 * 1024)
#define		OUTBUFNUM				(47 * 3)

// #define		UNIQUE_ID_PCR			0
#define		UNIQUE_ID_PAT			1
#define		UNIQUE_ID_PMT			2
#define		UNIQUE_ID_SIT			3

#define		DEFAULT_INPACKETSIZE	192
#define		DEFAULT_OUTPACKETSIZE	192

#define		MAX_PSI_SIZE			4096								// max. 
#define		MAX_PROGRAM_COUNT		252									// ((1024 - 12) / 4) - 1

#define		PROGINFOPOSITION		50									// �ԑg��񌳃t�@�C������ԑg�����擾����ʒu(0�`99)
#define		SITPOSITION				50									// �ϊ����t�@�C�������{�ƂȂ�SIT���擾����ʒu(0�`99)
#define		DEFAULTPACKETLIMIT		0

#define		MAXBUFSIZE				65536
#define		MAX_EXTEVENTDESC_NUM	16

#define		NAMESTRING				L"\npinfocopy version 1.2 "

#ifdef _WIN64
	#define		NAMESTRING2				L"(64bit)\n"
#else
	#define		NAMESTRING2				L"(32bit)\n"
#endif


// �\���̐錾

typedef struct {
	int		argPinfo;
	int		argSrc;
	int		argDest;
	int		argChannelName;
//	int		argChannelNum;
	int		argProgTime;
	int		argProgName;
	int		argProgDetail;
	int		argProgExtend[MAX_EXTEVENTDESC_NUM];
	int		numProgExtend;
	int		argProgGenre[3];
	int		numProgGenre;
	int		nInPacketSize;
	int		nOutPacketSize;
	int		ts_startpos;
	int		packet_limit;
	int		inbufnum;
} CopyParams;


// �v���g�^�C�v�錾

void	initCopyParams(CopyParams*, CopyOptions*);
BOOL	parseCopyOptions(int, _TCHAR*[], CopyParams*, CopyOptions*);
BOOL	parseCopyParams(_TCHAR*[], CopyParams*, CopyOptions*, ProgInfoDescriptor*);
int		strNewLine(WCHAR*, const int, WCHAR*, const int);


// ���C��

int _tmain(int argc, _TCHAR* argv[])
{
	_tsetlocale(LC_ALL, _T(""));


	// �p�����[�^�`�F�b�N

	if (argc == 1) {
		wprintf(NAMESTRING NAMESTRING2);
		exit(1);
	}

	CopyParams		param;
	CopyOptions		opt;
	initCopyParams(&param, &opt);

	if( !parseCopyOptions(argc, argv, &param, &opt) ) exit(1);															// �p�����[�^�ُ�Ȃ�I��


	// �ԑg���TS�t�@�C�����J���Ĕԑg����ǂݍ���, ���邢�̓R�}���h���C���w�肩��̔ԑg���̎擾

	ProgInfoDescriptor	proginfo;
	memset(&proginfo, 0, sizeof(ProgInfoDescriptor)); 

	if(param.argPinfo != 0) {
		if(!readFileProgInfo(argv[param.argPinfo], &proginfo, param.ts_startpos, param.packet_limit, &opt)) exit(1);		// �t�@�C�����J���Ĕԑg�����擾�A���s������I��
	} else {
		if(!parseCopyParams(argv, &param, &opt, &proginfo)) exit(1);														// �R�}���h���C������ԑg�����擾�A���s������I��
	}


	// �ϊ�TS���t�@�C���ǂݍ��݃N���X�쐬

	TSFileRead		TSin(argv[param.argSrc], INBUFSIZE, param.inbufnum, param.nInPacketSize, opt.bNoBuf);

	TSin.seek(0);
	TSin.showPacketError(FALSE);


	// �ϊ���TS�t�@�C������PAT�̎擾��PMT_PID�̒��o�@
	// -N �I�v�V�������w�肳��Ă���ꍇPAT, PMT �̏����������K�v
	// -T �I�v�V�������w�肳��Ă���ꍇPMT����PCR_PID���K�v

	unsigned char	psibuf[MAX_PSI_SIZE + 1];
	int				pmtpid[MAX_PROGRAM_COUNT];																	// PAT�ɗ񋓂����PMT_PID
	int				programcount = 0;																			// PMT_PID�̌�

	memset(psibuf, 0, sizeof(psibuf));
	memset(pmtpid, 0, sizeof(pmtpid));

	int		patlen = getPsiPacket(&TSin, psibuf, PID_PAT, param.nInPacketSize);
	if(patlen == 0) {
		wprintf(L"PAT�p�P�b�g�����o�ł��܂���ł���.\n�ϊ����t�@�C�� %s �̃p�P�b�g�T�C�Y(188/192)���Ċm�F���ĉ�����.\n", argv[param.argSrc]);
		exit(1);
	}

	programcount = parsePat(psibuf, pmtpid);
	if(programcount == 0) {
		wprintf(L"PAT����L����PMT_PID�����o�ł��܂���ł���.\n�ϊ����t�@�C�� %s �͐����TS�t�@�C���ł͖����\��������܂�.\n", argv[param.argSrc]);
		exit(1);
	}


	// �ϊ���TS�t�@�C������PMT�̎擾��PCR_PID�̒��o

	int		pcrpid = 0;

	memset(psibuf, 0, sizeof(psibuf));

	int		pmtlen = getPsiPacket(&TSin, psibuf, pmtpid[0], param.nInPacketSize);
	if(pmtlen == 0) {
		wprintf(L"�L����PMT�p�P�b�g�����o�ł��܂���ł���.\n�ϊ����t�@�C�� %s �͐����TS�t�@�C���ł͖����\��������܂�.\n", argv[param.argSrc]);
		exit(1);
	}

	parsePmt(psibuf, &pcrpid, NULL, NULL, NULL, FALSE, FALSE);


	// �ϊ���TS�t�@�C������SIT�擾

	TSin.seek(TSin.filesize * SITPOSITION / 100);															// default�ł̓t�@�C���̒��ԕt�߂���擾

	memset(psibuf, 0, sizeof(psibuf));

	int	sitlen = getPsiPacket(&TSin, psibuf, PID_SIT, param.nInPacketSize);		
	if(sitlen == 0) {
		wprintf(L"SIT�p�P�b�g�����o�ł��܂���ł���.\n�ϊ����t�@�C�� %s �͐���ȃp�[�V����TS�ł͖����\��������܂�.\n", argv[param.argSrc]);
		exit(1);
	}


	// SIT�ϊ��p�N���X�쐬

	SitProcess		sit(PID_SIT, pcrpid, &opt);

	int	result = sit.makeNewSit(psibuf, &proginfo);															// �ϊ���TS�t�@�C����SIT�ɁA�ԑg���proginfo�̊e�v�f��ǉ����ĐV����SIT���쐬����
	if(result != SIT_MAKE_NEW_SIT_SUCCESS) {
		wprintf(L"�ԑg���̒ǉ��Ɏ��s���܂���. �쐬�����SIT���T�C�Y�����𒴂��܂�.\n");			// SIT section�� > 4093 �Ŏ��s
		exit(1);
	}


	// PAT, PMT�ϊ��p�N���X�쐬

	PatProcess		pat(PID_PAT, proginfo.tsID, proginfo.serviceID, UNIQUE_ID_PAT);							// �g�p���Ȃ��ꍇ������i!bChannelNum�̎��j
	PmtProcess		pmt(pmtpid[0], proginfo.serviceID, UNIQUE_ID_PMT);										// �g�p���Ȃ��ꍇ������i!bChannelNum�̎��j


	// �t�@�C���������݃N���X�쐬

	TSFileWriteMultipleLock		TSout(argv[param.argDest], OUTBUFSIZE, OUTBUFNUM, param.nOutPacketSize, opt.bNoBuf);


	// ���C�����[�v

	TSin.seek(0);
	TSin.showPacketError(TRUE);

	DWORD		dwTime1 = GetTickCount();

	wprintf(L"�ϊ����J�n���܂�.\n");

	int			procpercent = 0;
	if(!opt.bDEBUG) wprintf(L"�ϊ���(0%%).\r");

	while(1)
	{
		// �f�[�^�ǂݍ��݂Əo��

		unsigned char	*bufin = TSin.read();
		if(bufin == NULL) break;												// NULL�Ȃ�t�@�C���I�[�܂œǂ񂾂̂ŏI��

		unsigned char	*buf = bufin + (param.nInPacketSize - 188);				// buf��0x47�w�b�_�̈ʒu

		unsigned char	*bufout = TSout.write();
		memcpy(bufout, bufin + param.nInPacketSize - param.nOutPacketSize, param.nOutPacketSize);	// 192->192, 188->188


		// PAT�Ɋւ��鏈��

		if(opt.bChannelNum)
		{
			result = pat.getPacket(buf, bufout + param.nOutPacketSize - 188);
			
			switch(result)
			{
				case PAT_NOT_PAT_PACKET:
					break;
				case PAT_NOT_STARTED:
					break;
				case PAT_STARTED:
					TSout.lock(pat.uid);
					break;
				case PAT_CONTINUED:
					break;
				case PAT_SINGLE_PACKET_CRC_ERROR:
					break;
				case PAT_MULTI_PACKET_CRC_ERROR:
					TSout.unlock(pat.uid);
					break;
				case PAT_SINGLE_PACKET_CONVED:
					if(opt.bDEBUG) wprintf(L"a");
					break;
				case PAT_MULTI_PACKET_CONVED:
					if(opt.bDEBUG) wprintf(L"A");
					TSout.unlock(pat.uid);
					break;
				case PAT_LIFE_EXPIRED:
					TSout.unlock(pat.uid);
					break;
				default:
					break;
			}
		}


		// PMT�Ɋւ��鏈��

		if(opt.bChannelNum)
		{
			result = pmt.getPacket(buf, bufout + param.nOutPacketSize - 188);
			
			switch(result)
			{
				case PMT_NOT_PMT_PACKET:
					break;
				case PMT_NOT_STARTED:
					break;
				case PMT_STARTED:
					TSout.lock(pmt.uid);
					break;
				case PMT_CONTINUED:
					break;
				case PMT_SINGLE_PACKET_CRC_ERROR:
					break;
				case PMT_MULTI_PACKET_CRC_ERROR:
					TSout.unlock(pmt.uid);
					break;
				case PMT_SINGLE_PACKET_CONVED:
					if(opt.bDEBUG) wprintf(L"m");
					break;
				case PMT_MULTI_PACKET_CONVED:
					if(opt.bDEBUG) wprintf(L"M");
					TSout.unlock(pmt.uid);
					break;
				case PMT_LIFE_EXPIRED:
					TSout.unlock(pmt.uid);
					break;
				default:
					break;
			}
		}


		// SIT�Ɋւ��鏈��

		result = sit.getPacket(buf, bufout + param.nOutPacketSize - 188);

		switch(result)
		{
			case SIT_NOT_SIT_PACKET:
				break;
			case SIT_REPLACEMENT_SUCCESS:
				if(opt.bDEBUG) wprintf(L"s");
				break;
			default:
				break;
		}


		// �i���󋵕\��

		if(!opt.bDEBUG) {
			int	newpercent = (int)((TSin.readpoint * 100) / TSin.filesize);
			if(newpercent > procpercent) {
				wprintf(L"�ϊ���(%d%%).    \r", newpercent);
				procpercent = newpercent;
			}
		}

	}


	// �o�b�t�@�Ɏc�����f�[�^�������o��

	TSout.finish();


	// �I������

	DWORD	dwTime2 = GetTickCount();
	wprintf(L"�t�@�C���I�[�܂Ő���ɕϊ����܂���.(%d sec.)\n", (dwTime2 - dwTime1) / 1000);

	wprintf(L"total read  %I64d bytes.\n", TSin.totalread);
	wprintf(L"total write %I64d bytes.\n", TSout.totalwrite);

	return 0;
}


void initCopyParams(CopyParams *param, CopyOptions *opt)
{
	opt->bChannelName		= FALSE;
	opt->bChannelNum		= FALSE;
	opt->bProgTime			= FALSE;
	opt->bProgName			= FALSE;
	opt->bProgDetail		= FALSE;
	opt->bProgExtend		= FALSE;
	opt->bProgGenre			= FALSE;
	opt->bVComponent		= FALSE;
	opt->bAComponent		= FALSE;
	opt->numAComponent		= 0;

	opt->bCharSizeFrom		= TRUE;
	opt->bNoBuf				= TRUE;
	opt->bDEBUG				= FALSE;

	param->argPinfo			= 0;
	param->argSrc			= 0;
	param->argDest			= 0;

	param->argChannelName	= 0;
//	param->argChannelNum	= 0;
	param->argProgTime		= 0;
	param->argProgName		= 0;
	param->argProgDetail	= 0;
	memset(param->argProgExtend, 0, sizeof(param->argProgExtend));
	param->numProgExtend	= 0;
	memset(param->argProgGenre, 0, sizeof(param->argProgGenre));
	param->numProgGenre		= 0;

	param->nInPacketSize	= DEFAULT_INPACKETSIZE;
	param->nOutPacketSize	= DEFAULT_OUTPACKETSIZE;
	param->ts_startpos		= PROGINFOPOSITION;
	param->packet_limit		= DEFAULTPACKETLIMIT;
	param->inbufnum			= INBUFNUM;
}


BOOL parseCopyOptions(int argn, _TCHAR *args[], CopyParams *param, CopyOptions *opt)
{
	BOOL	bCopy			= FALSE;
	BOOL	bSet			= FALSE;

	BOOL	bArgSkip		= FALSE;
	BOOL	bArgSkip2		= FALSE;
	BOOL	bArgSkip3		= FALSE;

	for(int i = 1; i < argn; i++) {

		if( args[i][0] == L'-') {

			for(int j = 1; j < (int)wcslen(args[i]); j++) {

				BOOL	bOneParam = (i < (argn - 1)) ? TRUE : FALSE;
				BOOL	bTwoParam = (i < (argn - 2)) ? TRUE : FALSE;
				BOOL	bThreeParam = (i < (argn - 3)) ? TRUE : FALSE;

				switch(args[i][j])
				{
					case L'C':
						opt->bChannelName = TRUE;
						bCopy = TRUE;
						break;
					case L'N':
						opt->bChannelNum = TRUE;
						bCopy = TRUE;
						break;
					case L'T':
						opt->bProgTime = TRUE;
						bCopy = TRUE;
						break;
					case L'B':
						opt->bProgName = TRUE;
						bCopy = TRUE;
						break;
					case L'I':
						opt->bProgDetail = TRUE;
						bCopy = TRUE;
						break;
					case L'E':
						opt->bProgExtend = TRUE;
						bCopy = TRUE;
						break;
					case L'G':
						opt->bProgGenre = TRUE;
						bCopy = TRUE;
						break;
					case L'V':
						opt->bVComponent = TRUE;
						bCopy = TRUE;
						break;
					case L'A':
						if( !bOneParam || (_wtoi(args[i + 1]) < 0) ) {
							wprintf(L"-A �I�v�V�����̈������ُ�ł�.\n");
							return FALSE;
						}
						opt->bAComponent = TRUE;
						opt->numAComponent = _wtoi(args[i + 1]);
						bCopy = TRUE;
						bArgSkip = TRUE;
						break;
						
					case L'c':
						param->argChannelName = bOneParam ? i + 1 : -1;
						opt->bChannelName = TRUE;
						bSet = TRUE;
						bArgSkip = TRUE;
						break;
//					case L'n':
//						param->argChannelNum = bOneParam ? i + 1 : -1;
//						opt->bChannelNum;
//						bSet = TRUE;
//						bArgSkip = TRUE;
//						break;
					case L't':
						param->argProgTime = bThreeParam ? i + 1 : -1;
						opt->bProgTime = TRUE;
						bSet = TRUE;
						bArgSkip3 = TRUE;
						break;
					case L'b':
						param->argProgName = bOneParam ? i + 1 : -1;
						opt->bProgName = TRUE;
						bSet = TRUE;
						bArgSkip = TRUE;
						break;
					case L'i':
						param->argProgDetail = bOneParam ? i + 1 : -1;
						opt->bProgDetail = TRUE;
						bSet = TRUE;
						bArgSkip = TRUE;
						break;
					case L'e':
						if(param->numProgExtend != MAX_EXTEVENTDESC_NUM) param->argProgExtend[param->numProgExtend++] = bTwoParam ? i + 1 : -1;
						opt->bProgExtend = TRUE;
						bSet = TRUE;
						bArgSkip2 = TRUE;
						break;
					case L'g':
						if(param->numProgGenre != 3) param->argProgGenre[param->numProgGenre++] = bOneParam ? i + 1 : -1;
						opt->bProgGenre = TRUE;
						bSet = TRUE;
						bArgSkip = TRUE;
						break;

					case L'f':
						if( !bOneParam || (_wtoi(args[i + 1]) < 0) || (_wtoi(args[i + 1]) > 99) ) {
							wprintf(L"-f �I�v�V�����̈������ُ�ł�.\n");
							return FALSE;
						}
						param->ts_startpos = _wtoi(args[i + 1]);
						bArgSkip = TRUE;
						break;
					case L'v':
						if( !bOneParam || (_wtoi(args[i + 1]) <= 0) ) {
							wprintf(L"-v �I�v�V�����̈������ُ�ł�.\n");
							return FALSE;
						}
						param->inbufnum = _wtoi(args[i + 1]);
						bArgSkip = TRUE;
						break;
					case L'u':
						param->nInPacketSize	= 188;
						param->nOutPacketSize	= 188;
						break;
					case L'l':
						if( !bOneParam || (_wtoi(args[i + 1]) <= 0) ) {
							wprintf(L"-l �I�v�V�����̈������ُ�ł�.\n");
							return FALSE;
						}
						param->packet_limit = _wtoi(args[i + 1]);
						bArgSkip = TRUE;
						break;
					case L'x':
						opt->bNoBuf = FALSE;
						break;
					case L'd':
						opt->bDEBUG = TRUE;
						break;
					case L'w':
						opt->bCharSizeFrom = FALSE;
						break;
					default:
						wprintf(L"�����ȃX�C�b�`���w�肳��܂���.\n");
						return FALSE;
				}
			}
		
		} else {

			if(param->argPinfo == 0) {
				param->argPinfo = i;
			} 
			else if(param->argSrc == 0) {
				param->argSrc = i;
			} 
			else if(param->argDest == 0) {
				param->argDest = i;
			}
			else {
				wprintf(L"�p�����[�^���������܂�.\n");
				return FALSE;
			}
		}

		if(bArgSkip){
			i++;
		} else if(bArgSkip2) {
			i += 2;
		} else if(bArgSkip3) {
			i += 3;
		}

		bArgSkip = FALSE;
		bArgSkip2 = FALSE;
		bArgSkip3 = FALSE;

	}

	if(!bCopy && !bSet){
		wprintf(L"�R�s�[����I�v�V�������w�肵�ĉ�����.\n");
		return	FALSE;
	}

	if(bCopy && bSet) {
		wprintf(L"�R�s�[�I�v�V�����ƃp�����[�^�w��I�v�V�����͓����w��ł��܂���.\n");
		return FALSE;
	}

	if( (bCopy && (param->argDest == 0)) || (bSet && (param->argSrc == 0)) ) {
		wprintf(L"�p�����[�^������܂���.\n");
		return FALSE;
	}

	if( bSet && (param->argDest != 0) ) {
		wprintf(L"�p�����[�^���������܂�.\n");
		return	FALSE;
	}

	if(bSet) {
		param->argDest = param->argSrc;
		param->argSrc = param->argPinfo;
		param->argPinfo = 0;
	}

	return TRUE;
}


BOOL parseCopyParams(_TCHAR *args[], CopyParams *param, CopyOptions *opt, ProgInfoDescriptor *info)
{
	if(opt->bChannelName)
	{
		if(param->argChannelName == -1) {
			wprintf(L"-c �I�v�V�����̈������ُ�ł�.\n");
			return FALSE;
		}

		unsigned char	chname[20];
		int				chnamelen;
		
		chnamelen = conv_from_unicode(chname, 20, args[param->argChannelName], (int)wcslen(args[param->argChannelName]), opt->bCharSizeFrom);
/*
		info->service[0] = 0x48;
		info->service[1] = (unsigned char)(3 + chnamelen * 2);
		info->service[2] = 0x01;
		info->service[3] = (unsigned char)chnamelen;
		memcpy(info->service + 4, chname, chnamelen);
		info->service[4 + chnamelen] = (unsigned char)chnamelen;
		memcpy(info->service + 5 + chnamelen, chname, chnamelen);
		info->serviceLen = 5 + chnamelen * 2;
*/
		info->service[0] = 0x48;
		info->service[1] = (unsigned char)(3 + chnamelen);
		info->service[2] = 0x01;
		info->service[3] = 0x00;
		info->service[4] = (unsigned char)chnamelen;
		memcpy(info->service + 5, chname, chnamelen);
		info->serviceLen = 5 + chnamelen;
	}

	if(opt->bProgTime)
	{
		int		year, month, day, hour, min, sec, durhour, durmin, dursec;

		if(param->argProgTime == -1) {
			wprintf(L"-t �I�v�V�����̈������ُ�ł�.\n");
			return FALSE;
		}

		if( (swscanf_s(args[param->argProgTime], L"%d/%d/%d", &year, &month, &day) != 3) || (year < 1900) || (year > 2100) || (month < 1) || (month > 12) || (day < 1) || (day > 31) ) {
			wprintf(L"-t �I�v�V�����̕������t�ݒ肪�ُ�ł�.\n");
			return FALSE;
		}

		if( (swscanf_s(args[param->argProgTime + 1], L"%d:%d:%d", &hour, &min, &sec) != 3) || (hour < 0) || (hour > 23) || (min < 0) || (min > 59) || (sec < 0) || (sec > 59) ) {
			wprintf(L"-t �I�v�V�����̕��������ݒ肪�ُ�ł�.\n");
			return FALSE;
		}

		if( (swscanf_s(args[param->argProgTime + 2], L"%d:%d:%d", &durhour, &durmin, &dursec) != 3) || (durhour < 0) || (durhour > 23) || (durmin < 0) || (durmin > 59) || (dursec < 0) || (dursec > 59) ) {
			wprintf(L"-t �I�v�V�����̕������Ԑݒ肪�ُ�ł�.\n");
			return FALSE;
		}

		int		mjd = mjd_enc(year, month, day);

		info->partialTsTime[0x00] = 0xC3;						// �p�[�V�����g�����X�|�[�g�X�g���[���^�C���L�q�q�쐬
		info->partialTsTime[0x01] = 18;
		info->partialTsTime[0x02] = 0x00;
		info->partialTsTime[0x03] = (mjd & 0xFF00) >> 8;
		info->partialTsTime[0x04] = mjd & 0x00FF;
		info->partialTsTime[0x05] = ((hour / 10) << 4) + (hour % 10);
		info->partialTsTime[0x06] = ((min / 10) << 4) + (min % 10);
		info->partialTsTime[0x07] = ((sec / 10) << 4) + (sec % 10);
		info->partialTsTime[0x08] = ((durhour / 10) << 4) + (durhour % 10);
		info->partialTsTime[0x09] = ((durmin / 10) << 4) + (durmin % 10);
		info->partialTsTime[0x0A] = ((dursec / 10) << 4) + (dursec % 10);
		info->partialTsTime[0x0B] = 0x00;
		info->partialTsTime[0x0C] = 0x00;
		info->partialTsTime[0x0D] = 0x00;
		info->partialTsTime[0x0E] = 0xF9;
		info->partialTsTime[0x0F] = 0x00;
		info->partialTsTime[0x10] = 0x00;
		info->partialTsTime[0x11] = 0x00;
		info->partialTsTime[0x12] = 0x00;
		info->partialTsTime[0x13] = 0x00;

		info->partialTsTimeLen = 20;
	}

	unsigned char	pname[80];
	unsigned char	pdetail[160];

	int				pnamelen = 0;
	int				pdetaillen = 0;

	WCHAR			wstr[MAXBUFSIZE];
	int				wlen;

	if(opt->bProgName)
	{
		if(param->argProgName == -1) {
			wprintf(L"-b �I�v�V�����̈������ُ�ł�.\n");
			return FALSE;
		}

		pnamelen = conv_from_unicode(pname, 80, args[param->argProgName], (int)wcslen(args[param->argProgName]), opt->bCharSizeFrom);
	}
	
	if(opt->bProgDetail)
	{
		if(param->argProgDetail == -1) {
			wprintf(L"-i �I�v�V�����̈������ُ�ł�.\n");
			return FALSE;
		}

		wlen = strNewLine(wstr, MAXBUFSIZE, args[param->argProgDetail], (int)wcslen(args[param->argProgDetail]));
		pdetaillen = conv_from_unicode(pdetail, 160, wstr, wlen, opt->bCharSizeFrom);
	}

	if( (pnamelen != 0) || (pdetaillen != 0) )
	{
		info->shortEvent[0] = 0x4D;
		info->shortEvent[1] = (unsigned char)(5 + pnamelen + pdetaillen);
		info->shortEvent[2] = 0x6A;
		info->shortEvent[3] = 0x70;
		info->shortEvent[4] = 0x6E;
		info->shortEvent[5] = (unsigned char)pnamelen;
		memcpy(info->shortEvent + 6, pname, pnamelen);
		info->shortEvent[6 + pnamelen] = (unsigned char)pdetaillen;
		memcpy(info->shortEvent + 7 + pnamelen,  pdetail, pdetaillen);
		info->shortEventLen = 7 + pnamelen + pdetaillen;
	}

	if(opt->bProgExtend)
	{
		unsigned char	name[16];
		unsigned char	description[200];

		int				namelen = 0;
		int				descriptionlen = 0;

		int				len = 0;

		for(int i = 0; i < param->numProgExtend; i++)
		{
			if(param->argProgExtend[i] == -1) {
				wprintf(L"-e �I�v�V�����̈������ُ�ł�.\n");
				return FALSE;
			}

			namelen = 0;
			if(wcscmp(args[param->argProgExtend[i]], L"none") != 0) namelen = conv_from_unicode(name, 16, args[param->argProgExtend[i]], (int)wcslen(args[param->argProgExtend[i]]), opt->bCharSizeFrom);
			wlen = strNewLine(wstr, MAXBUFSIZE, args[param->argProgExtend[i] + 1], (int)wcslen(args[param->argProgExtend[i] + 1]));
			descriptionlen = conv_from_unicode(description, 200, wstr, wlen, opt->bCharSizeFrom);

			info->extendedEvent[len + 0] = 0x4E;
			info->extendedEvent[len + 1] = (unsigned char)(8 + namelen + descriptionlen);
			info->extendedEvent[len + 2] = ((i & 0x0F) << 4) + ((param->numProgExtend - 1) & 0x0F);
			info->extendedEvent[len + 3] = 0x6A;
			info->extendedEvent[len + 4] = 0x70;
			info->extendedEvent[len + 5] = 0x6E;
			info->extendedEvent[len + 6] = (unsigned char)(2 + namelen + descriptionlen);
			info->extendedEvent[len + 7] = (unsigned char)namelen;
			memcpy(info->extendedEvent + len + 8, name, namelen);
			info->extendedEvent[len + 8 + namelen] = (unsigned char)descriptionlen;
			memcpy(info->extendedEvent + len + 9 + namelen, description, descriptionlen);
			info->extendedEvent[len + 9 + namelen + descriptionlen] = 0x00;
			len += (10 + namelen + descriptionlen);
		}

		info->extendedEventLen = len;
	}

	if(opt->bProgGenre)
	{
		info->content[0] = 0x54;
		info->content[1] = param->numProgGenre * 2;

		for(int i = 0; i < param->numProgGenre; i++)
		{
			if( (param->argProgGenre[i] == -1) || (wcstol(args[param->argProgGenre[i]], NULL, 0) < 0) ||  (wcstol(args[param->argProgGenre[i]], NULL, 0) > 0xFF) ) {
				wprintf(L"-g �I�v�V�����̈������ُ�ł�.\n");
				return FALSE;
			}

			info->content[2 + i * 2 + 0] = (unsigned char)wcstol(args[param->argProgGenre[i]], NULL, 0);
			info->content[2 + i * 2 + 1] = 0xFF;
		}
		
		info->contentLen = 2 + param->numProgGenre * 2;
	}

	return TRUE;
}


int strNewLine(WCHAR *dst, const int maxbufsize, WCHAR *src, const int srclen)
{
	WCHAR	*nlstr = L"\\n";									// ���s�ɕϊ�����ׂ�������

	int		spos = 0;
	int		dpos = 0;
	
	while(spos < srclen)
	{
		WCHAR	*nlpos = wcsstr(src + spos, nlstr);
		
		if(nlpos == NULL) break;

		while(spos < (int)(nlpos - src)) {
			if(dpos < maxbufsize) dst[dpos] = src[spos];
			spos++;
			dpos++;
		}

		spos += (int)wcslen(nlstr);

		if(dpos < maxbufsize) dst[dpos++] = 0x000D;				// ���ۂ̉��s�R�[�h
		if(dpos < maxbufsize) dst[dpos++] = 0x000A;
	}

	while(spos < srclen) {
		if(dpos < maxbufsize) dst[dpos] = src[spos];
		spos++;
		dpos++;
	}

	if(dpos < maxbufsize) dst[dpos] = 0x0000;
	if(dpos > maxbufsize) dpos = maxbufsize;

	return dpos;
}