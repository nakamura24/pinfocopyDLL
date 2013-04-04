#pragma once

// 定数

#define		MEDIATYPE_TB		0x5442
#define		MEDIATYPE_BS		0x4253
#define		MEDIATYPE_CS		0x4353
#define		MEDIATYPE_UNKNOWN	0xFFFF

#define		PID_PAT				0x0000
#define		PID_SIT				0x001f
#define		PID_EIT				0x0012
#define		PID_NIT				0x0010
#define		PID_SDT				0x0011
#define		PID_BIT				0x0024
#define		PID_INVALID			0xffff

#define		PSIBUFSIZE			65536
#define		MAXDESCSIZE			258
#define		MAXEXTEVENTSIZE		4096

#define		FILE_INVALID		0
#define		FILE_RPLS			1
#define		FILE_188TS			188
#define		FILE_192TS			192


//

typedef struct {
	BOOL	bChannelName;
	BOOL	bChannelNum;
	BOOL	bProgTime;
	BOOL	bProgName;
	BOOL	bProgDetail;
	BOOL	bProgExtend;
	BOOL	bProgGenre;
	BOOL	bVComponent;
	BOOL	bAComponent;
	int		numAComponent;
	BOOL	bNoBuf;
	BOOL	bDEBUG;
	BOOL	bCharSizeFrom;
} CopyOptions;


// 番組情報記述子構造体

typedef struct {
	unsigned char	networkIdentifier[MAXDESCSIZE];
	int				networkIdentifierLen;
	int				tsID;
	int				serviceID;
	unsigned char	partialTsTime[MAXDESCSIZE];
	int				partialTsTimeLen;
	unsigned char	content[MAXDESCSIZE];
	int				contentLen;
	unsigned char	service[MAXDESCSIZE];
	int				serviceLen;
	unsigned char	tsInformation[MAXDESCSIZE];
	int				tsInformationLen;
	unsigned char	shortEvent[MAXDESCSIZE];
	int				shortEventLen;
	unsigned char	extendedEvent[MAXEXTEVENTSIZE];
	int				extendedEventLen;
	unsigned char	extendedBroadcaster[MAXDESCSIZE];
	int				extendedBroadcasterLen;
	unsigned char	eventGroup[MAXDESCSIZE];
	int				eventGroupLen;
	unsigned char	broadcasterName[MAXDESCSIZE];
	int				broadcasterNameLen;
	unsigned char	component[MAXDESCSIZE];
	int				componentLen;
	unsigned char	audioComponent[MAXDESCSIZE * 2];
	int				audioComponentLen;
} ProgInfoDescriptor;


// プロトタイプ宣言

int				getSitEit(HANDLE, unsigned char*, const int, const int, int*, const int);
void			getSdtNitBit(HANDLE, unsigned char*, BOOL*, unsigned char*, BOOL*, unsigned char*, BOOL*, const int, const int, const int, const int);
void			mjd_dec(const int, int*, int*, int*);
int				mjd_enc(const int, const int, const int);
int				comparefornidtable(const void*, const void*);
int				getTbChannelNum(const int, const int, int);
void			parseSit(const unsigned char*, ProgInfoDescriptor*);
void			parseEit(const unsigned char*, ProgInfoDescriptor*);
void			parseSdt(const unsigned char*, ProgInfoDescriptor*);
void			parseNit(const unsigned char*, ProgInfoDescriptor*);
void			parseBit(const unsigned char*, ProgInfoDescriptor*);
int				rplsTsCheck(HANDLE);
BOOL			isTB(ProgInfoDescriptor*);
BOOL			readFileProgInfo(WCHAR*, ProgInfoDescriptor*, const int, const int, const CopyOptions*);
BOOL			readTsProgInfo(HANDLE, ProgInfoDescriptor*, const int, const int, const int, const CopyOptions*);

void			show_descriptor(const ProgInfoDescriptor*);
void			disp_code(const unsigned char*, const int);
