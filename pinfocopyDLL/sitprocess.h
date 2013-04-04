#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "proginfo.h"
#include "tsprocess.h"


// íËêî

#define		MAX_SIT_SIZE							4096
#define		SIT_MAX_PCR_TIME_INTERVAL				5000000		// Ç®ÇÊÇª 150 msec


#define		SIT_MAKE_NEW_SIT_SUCCESS				0
#define		SIT_TRANSMISSION_INFO_LOOP_TOO_LONG		1
#define		SIT_SERVICE_LOOP_TOO_LONG				2
#define		SIT_SECTION_TOO_LONG					3
#define		SIT_NOT_SIT_PACKET						16
#define		SIT_REPLACEMENT_SUCCESS					17


// SITèàóùÉNÉâÉX

class	SitProcess
{
public:
	int			pid;
//	int			uid;

	SitProcess(const int npid, const int npcrpid, const CopyOptions *opt);
// 	~SitProcess();

	int					makeNewSit(const unsigned char *psibuf, const ProgInfoDescriptor *proginfo);
	int					getPacket(const unsigned char *packet, unsigned char *wbuf);

private:
	unsigned char		sitbuf[MAX_SIT_SIZE * 2];

	int					buflen;
	int					sitnum;
	int					sitcount;

	BOOL				bChNum;
	BOOL				bChName;
	BOOL				bProgTime;
	BOOL				bProgName;
	BOOL				bProgDetail;
	BOOL				bProgExt;
	BOOL				bProgGenre;
	BOOL				bVComponent;
	BOOL				bAComponent;
	int					numAComponent;

	int					pcrpid;
	int					ptst_desc_pos;
	__int64				pcr_counter;
	__int64				prev_pcr;
	BOOL				bPcrStarted;
	int					time_mjd;
	int					time_sec;
	int					time_sec_old;
	int					version_number;
};