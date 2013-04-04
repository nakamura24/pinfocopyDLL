//
#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tsprocess.h"
#include "proginfo.h"

// #include "convtounicode.h"
// #include <conio.h>


//

int getSitEit(HANDLE hFile, unsigned char *psibuf, const int psize, const int startpos, int *ts_id, const int max_packet_count)
{
	unsigned char	*buf;
	unsigned char	sitbuf[PSIBUFSIZE];
	unsigned char	eitbuf[PSIBUFSIZE];
	unsigned char	patbuf[PSIBUFSIZE];

	memset(sitbuf, 0, PSIBUFSIZE);
	memset(eitbuf, 0, PSIBUFSIZE);
	memset(patbuf, 0, PSIBUFSIZE);

	BOOL		bSitFound	= FALSE;
	BOOL		bEitFound	= FALSE;
	BOOL		bPatFound	= FALSE;

	BOOL		bSitStarted	= FALSE;
	BOOL		bEitStarted	= FALSE;
	BOOL		bPatStarted = FALSE;

	int			sitlen = 0;
	int			eitlen = 0;
	int			patlen = 0;

	int			service_id = 0;

	TsReadProcess	tsin;
	initTsFileRead(&tsin, hFile, psize);
	setPosTsFileRead(&tsin, startpos);

	int			packet_count = 0;

	while(1)
	{
		buf = getPacketTsFileRead(&tsin);
		if(buf == NULL) break;
		buf += (psize - 188);

		int		pid		= getPid(buf);

		if( (pid == PID_SIT) || (pid == PID_EIT) || (pid == PID_PAT) )
		{
			BOOL	bPsiTop = isPsiTop(buf);
			int		adaplen = getAdapFieldLength(buf);
			int		pflen	= getPointerFieldLength(buf);						// !bPsiTopな場合は無意味な値

			int		len;
			BOOL	bTop = FALSE;
			BOOL	bFirstSection = TRUE;
			int		i = adaplen + 4;

			while(i < 188)														// 188バイトパケット内での各セクションのループ
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
					if(buf[i] == 0xFF) break;									// TableIDのあるべき場所が0xFF(stuffing byte)ならそのパケットに関する処理は終了
					len = getSectionLength(buf + i) + 3;						// セクションヘッダはパケットにまたがって配置されることは無いはずなのでパケット範囲外(188バイト以降)を読みに行くことは無いはず
					if(i + len > 188) len = 188 - i;
					bTop = TRUE;
				}

				// SITに関する処理

				if(pid == PID_SIT) {
					if(bSitStarted && !bTop) {
						memcpy(sitbuf + sitlen, buf + i, len);
						sitlen += len;
					}
					if(bTop) {
						memset(sitbuf, 0, sizeof(sitbuf));
						memcpy(sitbuf, buf + i, len);
						sitlen = len;
						bSitStarted = TRUE;
					}
					if( bSitStarted && (sitlen >= getSectionLength(sitbuf) + 3) ) {
						int		seclen = getSectionLength(sitbuf);
						int		crc = calc_crc32(sitbuf, seclen + 3);
						if( (crc == 0) && (sitbuf[0] == 0x7F) ) {
							memcpy(psibuf, sitbuf, seclen + 3);
							bSitFound = TRUE;
							break;
						}
						bSitStarted = FALSE;
					}
				}

				// EITに関する処理

				if(pid == PID_EIT) {
					if(bEitStarted && !bTop) {
						memcpy(eitbuf + eitlen, buf + i, len);
						eitlen += len;
					}
					if(bTop) {
						memset(eitbuf, 0, sizeof(eitbuf));
						memcpy(eitbuf, buf + i, len);
						eitlen = len;
						bEitStarted = TRUE;
					}
					if( bEitStarted && (eitlen >= getSectionLength(eitbuf) + 3) ) {
						int		seclen = getSectionLength(eitbuf);
						int		crc = calc_crc32(eitbuf, seclen + 3);
						int		sid = eitbuf[0x03] * 256 + eitbuf[0x04];
						if( (crc == 0) && (eitbuf[0] == 0x4E) && (eitbuf[0x06] == 0) && bPatFound && (sid == service_id) && (seclen > 15) ) {
							memcpy(psibuf, eitbuf, seclen + 3);
							bEitFound = TRUE;
							break;
						}
						bEitStarted = FALSE;
					}
				}

				// PATに関する処理

				if( (pid == PID_PAT) && !bPatFound ) {
					if(bPatStarted && !bTop) {
						memcpy(patbuf + patlen, buf + i, len);
						patlen += len;
					}
					if(bTop) {
						memset(patbuf, 0, sizeof(patbuf));
						memcpy(patbuf, buf + i, len);
						patlen = len;
						bPatStarted = TRUE;
					}
					if( bPatStarted && (patlen >= getSectionLength(patbuf) + 3) ) {
						int		seclen = getSectionLength(patbuf);
						int		crc = calc_crc32(patbuf, seclen + 3);
						if( (crc == 0) && (patbuf[0] == 0x00) ) {
							*ts_id = patbuf[0x03] * 256 + patbuf[0x04];
							int		j = 0x08;
							while(j < seclen - 1) {
								service_id = patbuf[j] * 256 + patbuf[j + 1];
								if(service_id != 0) {
									bPatFound = TRUE;
									break;
								}
								j += 4;
							}
						}
						bPatStarted = FALSE;
					}
				}

				if( bSitFound || bEitFound ) break;

				i += len;
			}
		}

		if( bSitFound || bEitFound ) break;

		if(max_packet_count != 0) {
			packet_count++;
			if(packet_count >= max_packet_count) break;
		}

	}

	if(bSitFound) return PID_SIT;
	if(bEitFound) return PID_EIT;

	return PID_INVALID;
}


void getSdtNitBit(HANDLE hFile, unsigned char *psibuf1, BOOL *bSdtFound, unsigned char *psibuf2, BOOL *bNitFound, unsigned char *psibuf3, BOOL *bBitFound, const int psize, const int serviceid, const int startpos, const int max_packet_count)
{
	unsigned char	*buf;
	unsigned char	sdtbuf[PSIBUFSIZE];
	unsigned char	nitbuf[PSIBUFSIZE];
	unsigned char	bitbuf[PSIBUFSIZE];

	BOOL		bSdtStarted	= FALSE;
	BOOL		bNitStarted	= FALSE;
	BOOL		bBitStarted	= FALSE;

	int			sdtlen		= 0;
	int			nitlen		= 0;
	int			bitlen		= 0;

	TsReadProcess	tsin;
	initTsFileRead(&tsin, hFile, psize);
	setPosTsFileRead(&tsin, startpos);

	int			packet_count = 0;

	while(1)
	{
		buf = getPacketTsFileRead(&tsin);
		if(buf == NULL) break;
		buf += (psize - 188);

		int		pid		= getPid(buf);

		if( (pid == PID_SDT) || (pid == PID_NIT) || (pid == PID_BIT) )
		{
			BOOL	bPsiTop = isPsiTop(buf);
			int		adaplen = getAdapFieldLength(buf);
			int		pflen	= getPointerFieldLength(buf);						// !bPsiTopな場合は無意味な値

			int		len;
			BOOL	bTop = FALSE;
			BOOL	bFirstSection = TRUE;
			int		i = adaplen + 4;

			while(i < 188)														// 188バイトパケット内での各セクションのループ
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
					if(buf[i] == 0xFF) break;									// TableIDのあるべき場所が0xFF(stuffing byte)ならそのパケットに関する処理は終了
					len = getSectionLength(buf + i) + 3;						// セクションヘッダはパケットにまたがって配置されることは無いはずなのでパケット範囲外(188バイト以降)を読みに行くことは無いはず
					if(i + len > 188) len = 188 - i;
					bTop = TRUE;
				}

				// SDTに関する処理

				if( (pid == PID_SDT) && !(*bSdtFound) ) {
					if(bSdtStarted && !bTop) {
						memcpy(sdtbuf + sdtlen, buf + i, len);
						sdtlen += len;
					}
					if(bTop) {
						memset(sdtbuf, 0, sizeof(sdtbuf));
						memcpy(sdtbuf, buf + i, len);
						sdtlen = len;
						bSdtStarted = TRUE;
					}
					if( bSdtStarted && (sdtlen >= getSectionLength(sdtbuf) + 3) ) {
						int		seclen = getSectionLength(sdtbuf);
						int		crc = calc_crc32(sdtbuf, seclen + 3);
						if( (crc == 0) && (sdtbuf[0] == 0x42) ) {
							int		j = 0x0B;
							while(j < seclen) {
								int		serviceID = sdtbuf[j] * 256 + sdtbuf[j + 1];
								int		slen = getLength(sdtbuf+ j + 0x03);
								if(serviceID == serviceid) {
									memcpy(psibuf1, sdtbuf, seclen + 3);
									*bSdtFound = TRUE;
									break;
								}
								j += (slen + 5);
							}
						}
						bSdtStarted = FALSE;
					}
				}

				// NITに関する処理

				if( (pid == PID_NIT) && !(*bNitFound) ) {
					if(bNitStarted && !bTop) {
						memcpy(nitbuf + nitlen, buf + i, len);
						nitlen += len;
					}
					if(bTop) {
						memset(nitbuf, 0, sizeof(nitbuf));
						memcpy(nitbuf, buf + i, len);
						nitlen = len;
						bNitStarted = TRUE;
					}
					if( bNitStarted && (nitlen >= getSectionLength(nitbuf) + 3) ) {
						int		seclen = getSectionLength(nitbuf);
						int		crc = calc_crc32(nitbuf, seclen + 3);
						if( (crc == 0) && (nitbuf[0] == 0x40) ) {
							memcpy(psibuf2, nitbuf, seclen + 3);
							*bNitFound = TRUE;
							break;
						}
						bNitStarted = FALSE;
					}
				}

				// BITに関する処理

				if( (pid == PID_BIT) && !(*bBitFound) ) {
						if(bBitStarted && !bTop) {
							memcpy(bitbuf + bitlen, buf + i, len);
							bitlen += len;
						}
						if(bTop) {
							memset(bitbuf, 0, sizeof(bitbuf));
							memcpy(bitbuf, buf + i, len);
							bitlen = len;
							bBitStarted = TRUE;
						}
						if( bBitStarted && (bitlen >= getSectionLength(bitbuf) + 3) ) {
							int		seclen = getSectionLength(bitbuf);
							int		crc = calc_crc32(bitbuf, seclen + 3);
							if( (crc == 0) && (bitbuf[0] == 0xC4) ) {
								memcpy(psibuf3, bitbuf, seclen + 3);
								*bBitFound = TRUE;
								break;
							}
							bBitStarted = FALSE;
						}
				}

				i += len;
			}
		}

		if(*bSdtFound && *bNitFound && *bBitFound) break;

		if(max_packet_count != 0) {
			packet_count++;
			if(packet_count >= max_packet_count) break;
		}

	}

	return;
}


void mjd_dec(const int mjd, int *recyear, int *recmonth, int *recday)
{
	int		mjd2 = (mjd < 15079) ? mjd + 0x10000 : mjd;

	int		year = (int)(((double)mjd2 - 15078.2) / 365.25);
	int		year2 = (int)((double)year  * 365.25);
	int		month = (int)(((double)mjd2 - 14956.1 - (double)year2) / 30.6001);
	int		month2 = (int)((double)month * 30.6001);
	int		day = mjd2 - 14956 - year2 - month2;

	if( (month == 14) || (month == 15) ) {
		year += 1901;
		month = month - 13;
	} else {
		year += 1900;
		month = month - 1;
	}

	*recyear = year;
	*recmonth = month;
	*recday = day;

	return;
}


int mjd_enc(const int year, const int month, const int day)
{
	int		year2 = (month > 2) ? year - 1900 : year - 1901;
	int		month2 = (month > 2) ? month + 1 : month + 13;

	int		a = (int)(365.25 * (double)year2);
	int		b = (int)(30.6001 * (double)month2);
	int		mjd = 14956 + day + a + b;

	return mjd & 0xFFFF;
}


int comparefornidtable(const void *item1, const void *item2)
{
	return *(int*)item1 - *(int*)item2;
}


int getTbChannelNum(const int networkID, const int serviceID, int remoconkey_id)
{
	static int	nIDTable[] = 
	{
		0x7FE0, 1,	0x7FE1, 2,	0x7FE2, 4,	0x7FE3, 6,	0x7FE4, 8,	0x7FE5, 5,	0x7FE6, 7,	0x7FE8, 12,		// 関東広域
		0x7FD1, 2,	0x7FD2, 4,	0x7FD3, 6,	0x7FD4, 8,	0x7FD5, 10,											// 近畿広域
		0x7FC1, 2,	0x7FC2, 1,	0x7FC3, 5,	0x7FC4, 6,	0x7FC5, 4,											// 中京広域			18局

		0x7FB2, 1,	0x7FB3, 5,	0x7FB4, 6,	0x7FB5, 8,	0x7FB6, 7,											// 北海道域
		0x7FA2, 4,	0x7FA3, 5,	0x7FA4, 6,	0x7FA5, 7,	0x7FA6, 8,											// 岡山香川
		0x7F92, 8,	0x7F93, 6,	0x7F94, 1,																	// 島根鳥取
		0x7F50, 3,	0x7F51, 2,	0x7F52, 1,	0x7F53, 5,	0x7F54, 6,	0x7F55, 8,	0x7F56, 7,					// 北海道（札幌）	20局

		0x7F40, 3,	0x7F41, 2,	0x7F42, 1,	0x7F43, 5,	0x7F44, 6,	0x7F45, 8,	0x7F46, 7,					// 北海道（函館）
		0x7F30, 3,	0x7F31, 2,	0x7F32, 1,	0x7F33, 5,	0x7F34, 6,	0x7F35, 8,	0x7F36, 7,					// 北海道（旭川）
		0x7F20, 3,	0x7F21, 2,	0x7F22, 1,	0x7F23, 5,	0x7F24, 6,	0x7F25, 8,	0x7F26, 7,					// 北海道（帯広）	21局

		0x7F10, 3,	0x7F11, 2,	0x7F12, 1,	0x7F13, 5,	0x7F14, 6,	0x7F15, 8,	0x7F16, 7,					// 北海道（釧路）
		0x7F00, 3,	0x7F01, 2,	0x7F02, 1,	0x7F03, 5,	0x7F04, 6,	0x7F05, 8,	0x7F06, 7,					// 北海道（北見）
		0x7EF0, 3,	0x7EF1, 2,	0x7EF2, 1,	0x7EF3, 5,	0x7EF4, 6,	0x7EF5, 8,	0x7EF6, 7,					// 北海道（室蘭）
		0x7EE0, 3,	0x7EE1, 2,	0x7EE2, 1,	0x7EE3, 8,	0x7EE4, 4,	0x7EE5, 5,								// 宮城				27局

		0x7ED0, 1,	0x7ED1, 2,	0x7ED2, 4,	0x7ED3, 8,	0x7ED4, 5,											// 秋田
		0x7EC0, 1,	0x7EC1, 2,	0x7EC2, 4,	0x7EC3, 5,	0x7EC4, 6,	0x7EC5, 8,								// 山形
		0x7EB0, 1,	0x7EB1, 2,	0x7EB2, 6,	0x7EB3, 4,	0x7EB4, 8,	0x7EB5, 5,								// 岩手
		0x7EA0, 1,	0x7EA1, 2,	0x7EA2, 8,	0x7EA3, 4,	0x7EA4, 5,	0x7EA5, 6,								// 福島				23局

		0x7E90, 3,	0x7E91, 2,	0x7E92, 1,	0x7E93, 6,	0x7E94, 5,											// 青森
		0x7E87, 9,																							// 東京
		0x7E77, 3,																							// 神奈川
		0x7E67, 3,																							// 群馬
		0x7E50, 1,																							// 茨城
		0x7E47, 3,																							// 千葉
		0x7E37, 3,																							// 栃木
		0x7E27, 3,																							// 埼玉
		0x7E10, 1,	0x7E11, 2,	0x7E12, 4,	0x7E13, 5,	0x7E14, 6,	0x7E15, 8,								// 長野
		0x7E00, 1,	0x7E01, 2,	0x7E02, 6,	0x7E03, 8,	0x7E04, 4,	0x7E05, 5,								// 新潟
		0x7DF0, 1,	0x7DF1, 2,	0x7DF2, 4,	0x7DF3, 6,														// 山梨				28局

		0x7DE0, 3,	0x7DE6, 10,																				// 愛知
		0x7DD0, 1,	0x7DD1, 2,	0x7DD2, 4,	0x7DD3, 5,	0x7DD4, 6,	0x7DD5, 8,								// 石川
		0x7DC0, 1,	0x7DC1, 2,	0x7DC2, 6,	0x7DC3, 8,	0x7DC4, 4,	0x7DC5, 5,								// 静岡
		0x7DB0, 1,	0x7DB1, 2,	0x7DB2, 7,	0x7DB3, 8,														// 福井
		0x7DA0, 3,	0x7DA1, 2,	0x7DA2, 1,	0x7DA3, 8,	0x7DA4, 6,											// 富山
		0x7D90, 3,	0x7D96, 7,																				// 三重
		0x7D80, 3,	0x7D86, 8,																				// 岐阜				27局

		0x7D70, 1,	0x7D76, 7,																				// 大阪
		0x7D60, 1,	0x7D66, 5,																				// 京都
		0x7D50, 1,	0x7D56, 3,																				// 兵庫
		0x7D40, 1,	0x7D46, 5,																				// 和歌山
		0x7D30, 1,	0x7D36, 9,																				// 奈良
		0x7D20, 1,	0x7D26, 3,																				// 滋賀
		0x7D10, 1,	0x7D11, 2,	0x7D12, 3,	0x7D13, 4,	0x7D14, 5,	0x7D15, 8,								// 広島
		0x7D00, 1,	0x7D01, 2,																				// 岡山
		0x7CF0, 3,	0x7CF1, 2,																				// 島根
		0x7CE0, 3,	0x7CE1, 2,																				// 鳥取				24局

		0x7CD0, 1,	0x7CD1, 2,	0x7CD2, 4,	0x7CD3, 3,	0x7CD4, 5,											// 山口
		0x7CC0, 1,	0x7CC1, 2,	0x7CC2, 4,	0x7CC3, 5,	0x7CC4, 6,	0x7CC5, 8,								// 愛媛
		0x7CB0, 1,	0x7CB1, 2,																				// 香川
		0x7CA0, 3,	0x7CA1, 2,	0x7CA2, 1,																	// 徳島
		0x7C90, 1,	0x7C91, 2,	0x7C92, 4,	0x7C93, 6,	0x7C94, 8,											// 高知				21局

		0x7C80, 3,	0x7C81, 2,	0x7C82, 1,	0x7C83, 4,	0x7C84, 5,	0x7C85, 7,	0x7C86, 8,					// 福岡
		0x7C70, 1,	0x7C71, 2,	0x7C72, 3,	0x7C73, 8,	0x7C74, 4,	0x7C75, 5,								// 熊本
		0x7C60, 1,	0x7C61, 2,	0x7C62, 3,	0x7C63, 8,	0x7C64, 5,	0x7C65, 4,								// 長崎
		0x7C50, 3,	0x7C51, 2,	0x7C52, 1,	0x7C53, 8,	0x7C54, 5,	0x7C55, 4,								// 鹿児島
		0x7C40, 1,	0x7C41, 2,	0x7C42, 6,	0x7C44, 3,														// 宮崎				29局

		0x7C30, 1,	0x7C31, 2,	0x7C32, 3,	0x7C33, 4,	0x7C34, 5,											// 大分
		0x7C20, 1,	0x7C21, 2,	0x7C22, 3,																	// 佐賀
		0x7C10, 1,	0x7C11, 2,	0x7C12, 3,	0x7C14, 5,	0x7C17, 8,											// 沖縄				13局

		0x7880, 3,	0x7881, 2,																				// 福岡（県域フラグ）	2局			計253局
	};

	static BOOL		bTableInit = FALSE;

	if(!bTableInit) {
		qsort(nIDTable, sizeof(nIDTable) / sizeof(int) / 2, sizeof(int) * 2, comparefornidtable);
		bTableInit = TRUE;
	}
	
	if(remoconkey_id == 0) {
		void *result = bsearch(&networkID, nIDTable, sizeof(nIDTable) / sizeof(int) / 2, sizeof(int) * 2, comparefornidtable);
		if(result == NULL) return 0;
		remoconkey_id = *((int*)result + 1);
	}

	return remoconkey_id * 10 + (serviceID & 0x0007) + 1;
}


void parseSit(const unsigned char *sitbuf, ProgInfoDescriptor *proginfo)
{
	int		totallen = getSectionLength(sitbuf);
	int		firstlen = getLength(sitbuf + 0x08);

	// firstloop処理

	int			mediaType	= MEDIATYPE_UNKNOWN;
	int			networkID	= 0;

	int			i = 0x0A;

	while(i < 0x0A + firstlen)
	{
		switch(sitbuf[i])
		{
			case 0xC2:													// ネットワーク識別記述子
				mediaType = sitbuf[i + 5] * 256 + sitbuf[i + 6];
				networkID = sitbuf[i + 7] * 256 + sitbuf[i + 8];
				proginfo->networkIdentifierLen = sitbuf[i + 1] + 2;
				memcpy(proginfo->networkIdentifier, sitbuf + i, proginfo->networkIdentifierLen);
				i += (sitbuf[i + 1] + 2);
				break;
			case 0xCD:													// TS情報記述子
				proginfo->tsInformationLen = sitbuf[i + 1] + 2;
				memcpy(proginfo->tsInformation, sitbuf + i, proginfo->tsInformationLen);
				i += (sitbuf[i + 1] + 2);
				break;
			default:
				i += (sitbuf[i + 1] + 2);
		}
	}


	// secondloop処理

	if( (mediaType == MEDIATYPE_CS) && ( (networkID == 0x0001) || (networkID == 0x0003) ) )		// スカパー PerfecTV & SKY サービス
	{
		while(i < totallen - 1)
		{
			proginfo->serviceID = sitbuf[i] * 256 + sitbuf[i + 1];
			int		secondlen = getLength(sitbuf + i + 2);

			i += 4;

			int		j = i;

			while(i < secondlen + j)
			{
				switch(sitbuf[i])
				{
					case 0xC3:													// パーシャルトランスポートストリームタイム記述子
						proginfo->partialTsTimeLen = 20;												// JST_timeを使うので長さ20バイト固定
						memcpy(proginfo->partialTsTime, sitbuf + i, proginfo->partialTsTimeLen);
						proginfo->partialTsTime[0x01] = (unsigned char)proginfo->partialTsTimeLen - 2;	// 記述子長固定
						proginfo->partialTsTime[0x0E] &= 0xFD;											// other_descriptor_statusのクリア
						proginfo->partialTsTime[0x0E] |= 0x01;											// JST_time_flagのセット
						i += (sitbuf[i + 1] + 2);
						break;
					case 0xB2:													// 局名と番組名に関する記述子
						if(sitbuf[i + 3] == 0x01) {
							unsigned char	servicenamelen	= sitbuf[i + 2] - 1;
							unsigned char	prognamelen		= sitbuf[i + 4 + servicenamelen] - 1;

							proginfo->service[0x00] = 0x48;												// サービス記述子作成
							proginfo->service[0x01] = servicenamelen + 3;
							proginfo->service[0x02] = 0x01;
							proginfo->service[0x03] = 0x00;
							proginfo->service[0x04] = servicenamelen;
							memcpy(proginfo->service + 5, sitbuf + i + 4, servicenamelen);
							proginfo->serviceLen = servicenamelen + 5;

							proginfo->shortEvent[0x00] = 0x4D;											// 短形式イベント記述子作成
							proginfo->shortEvent[0x01] = prognamelen + 5;
							proginfo->shortEvent[0x02] = 0x6A;
							proginfo->shortEvent[0x03] = 0x70;
							proginfo->shortEvent[0x04] = 0x6E;
							proginfo->shortEvent[0x05] = prognamelen;
							memcpy(proginfo->shortEvent + 6, sitbuf + i + 6 + servicenamelen, prognamelen);
							proginfo->shortEvent[prognamelen + 6] = 0x00;
							proginfo->shortEventLen = prognamelen + 7;
						}
						i += (sitbuf[i + 1] + 2);
						break;
					case 0x83:													// 番組詳細情報に関する記述子(拡張形式イベント記述子への変換が必要)
						i += (sitbuf[i + 1] + 2);
						break;
					default:
						i += (sitbuf[i + 1] + 2);
				}
			}

		}
	}
	else // if( (mediaType == MEDIATYPE_BS) || (mediaType == MEDIATYPE_TB) || ( (mediaType == MEDIATYPE_CS) && (networkID == 0x000A) ) )		// 地上デジタル, BSデジタル, (スカパーHD？)
	{																																			// スカパーSD以外は地デジ, BSデジタルと同形式の番組情報として扱うようにしました

		while(i < totallen - 1)
		{
			proginfo->serviceID = sitbuf[i] * 256 + sitbuf[i + 1];				// サービスID

			int		secondlen = getLength(sitbuf + i + 2);

			i += 4;
			int		j = i;

			proginfo->extendedEventLen = 0;
			proginfo->audioComponentLen = 0;

			while(i < secondlen + j)
			{
				switch(sitbuf[i])
				{
					case 0xC3:													// パーシャルトランスポートストリームタイム記述子
						proginfo->partialTsTimeLen = 20;												// JST_timeを使うので長さ20バイト固定
						memcpy(proginfo->partialTsTime, sitbuf + i, proginfo->partialTsTimeLen);
						proginfo->partialTsTime[0x01] = (unsigned char)proginfo->partialTsTimeLen - 2;	// 記述子長固定
						proginfo->partialTsTime[0x0E] &= 0xFD;											// other_descriptor_statusのクリア
						proginfo->partialTsTime[0x0E] |= 0x01;											// JST_time_flagのセット
						i += (sitbuf[i + 1] + 2);
						break;
					case 0x54:													// コンテント記述子
						proginfo->contentLen = sitbuf[i + 1] + 2;
						memcpy(proginfo->content, sitbuf + i, proginfo->contentLen);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0x48:													// サービス記述子
						proginfo->serviceLen = sitbuf[i + 1] + 2;
						memcpy(proginfo->service, sitbuf + i, proginfo->serviceLen);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0x4D:													// 短形式イベント記述子
						proginfo->shortEventLen = sitbuf[i + 1] + 2;
						memcpy(proginfo->shortEvent, sitbuf + i, proginfo->shortEventLen);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0x4E:													// 拡張形式イベント記述子
						memcpy(proginfo->extendedEvent + proginfo->extendedEventLen, sitbuf + i, sitbuf[i + 1] + 2);
						proginfo->extendedEventLen += (sitbuf[i + 1] + 2);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0xD6:													// イベントグループ記述子
						proginfo->eventGroupLen = sitbuf[i + 1] + 2;
						memcpy(proginfo->eventGroup, sitbuf + i, proginfo->eventGroupLen);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0xD8:													// ブロードキャスタ名記述子
						proginfo->broadcasterNameLen = sitbuf[i + 1] + 2;
						memcpy(proginfo->broadcasterName, sitbuf + i, proginfo->broadcasterNameLen);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0xCE:													// 拡張ブロードキャスタ記述子
						proginfo->extendedBroadcasterLen = sitbuf[i + 1] + 2;
						memcpy(proginfo->extendedBroadcaster, sitbuf + i, proginfo->extendedBroadcasterLen);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0x50:													// コンポーネント記述子
						proginfo->componentLen = sitbuf[i + 1] + 2;
						memcpy(proginfo->component, sitbuf + i, proginfo->componentLen);
						i += (sitbuf[i + 1] + 2);
						break;
					case 0xC4:													// 音声コンポーネント記述子
						memcpy(proginfo->audioComponent + proginfo->audioComponentLen, sitbuf + i, sitbuf[i + 1] + 2);
						proginfo->audioComponentLen += (sitbuf[i + 1] + 2);
						i += (sitbuf[i + 1] + 2);
						break;
					default:
						i += (sitbuf[i + 1] + 2);
				}
			}
		}

	}
//	else			// MEDIATYPE_UNKNOWN, 不明なMEDIATYPE_CS
//	{
//		proginfo->serviceID = sitbuf[i] * 256 + sitbuf[i + 1];
//	}

	return;
}


void parseEit(const unsigned char *eitbuf, ProgInfoDescriptor *proginfo)
{
	int 	totallen	= getSectionLength(eitbuf);

	proginfo->serviceID	= eitbuf[0x03] * 256 + eitbuf[0x04];
	int		networkID	= eitbuf[0x0A] * 256 + eitbuf[0x0B];

	proginfo->networkIdentifier[0x00] = 0xC2;						// ネットワーク識別記述子作成
	proginfo->networkIdentifier[0x01] = 0x07;
	proginfo->networkIdentifier[0x02] = 0x4A;						// 'J'
	proginfo->networkIdentifier[0x03] = 0x50;						// 'P'
	proginfo->networkIdentifier[0x04] = 0x4E;						// 'N'
	if( (networkID >= 0x7880) && (networkID <= 0x7FE8) ) {
		proginfo->networkIdentifier[0x05] = 0x54;					// 'T'
		proginfo->networkIdentifier[0x06] = 0x42;					// 'B'
	} else if( (networkID == 0x0004) || (networkID == 0x0006) || (networkID == 0x0007) ) {
		proginfo->networkIdentifier[0x05] = 0x42;					// 'B'
		proginfo->networkIdentifier[0x06] = 0x53;					// 'S'
	} else {
		proginfo->networkIdentifier[0x05] = 0x43;					// 'C'
		proginfo->networkIdentifier[0x06] = 0x53;					// 'S'
	}
	proginfo->networkIdentifier[0x07] = eitbuf[0x0A];
	proginfo->networkIdentifier[0x08] = eitbuf[0x0B];
	proginfo->networkIdentifierLen = 9;


	// totalloop処理

	int		i = 0x0E;

	while(i < totallen - 1)
	{
		proginfo->partialTsTime[0x00] = 0xC3;						// パーシャルトランスポートストリームタイム記述子作成
		proginfo->partialTsTime[0x01] = 20 - 2;
		proginfo->partialTsTime[0x02] = 0x00;
		memcpy(proginfo->partialTsTime + 3, eitbuf + i + 2, 8);
		proginfo->partialTsTime[0x0B] = 0x00;
		proginfo->partialTsTime[0x0C] = 0x00;
		proginfo->partialTsTime[0x0D] = 0x00;
		proginfo->partialTsTime[0x0E] = 0xF9;
		proginfo->partialTsTimeLen = 20;

		int		seclen = getLength(eitbuf + i + 0x0A);

		i += 0x0C;

	// secondloop処理

		int		j = i;

		proginfo->extendedEventLen = 0;

		while(i < seclen + j)
		{
			switch(eitbuf[i])
			{
				case 0x54:													// コンテント記述子
					proginfo->contentLen = eitbuf[i + 1] + 2;
					memcpy(proginfo->content, eitbuf + i, proginfo->contentLen);
					i += (eitbuf[i + 1] + 2);
					break;
				case 0x4D:													// 短形式イベント記述子
					proginfo->shortEventLen = eitbuf[i + 1] + 2;
					memcpy(proginfo->shortEvent, eitbuf + i, proginfo->shortEventLen);
					i += (eitbuf[i + 1] + 2);
					break;
				case 0x4E:													// 拡張形式イベント記述子
					memcpy(proginfo->extendedEvent + proginfo->extendedEventLen, eitbuf + i, eitbuf[i + 1] + 2);
					proginfo->extendedEventLen += (eitbuf[i + 1] + 2);
					i += (eitbuf[i + 1] + 2);
					break;
				case 0xD6:													// イベントグループ記述子
					proginfo->eventGroupLen = eitbuf[i + 1] + 2;
					memcpy(proginfo->eventGroup, eitbuf + i, proginfo->eventGroupLen);
					i += (eitbuf[i + 1] + 2);
					break;
				case 0x50:													// コンポーネント記述子
					proginfo->componentLen = eitbuf[i + 1] + 2;
					memcpy(proginfo->component, eitbuf + i, proginfo->componentLen);
					i += (eitbuf[i + 1] + 2);
					break;
				case 0xC4:													// 音声コンポーネント記述子
					memcpy(proginfo->audioComponent + proginfo->audioComponentLen, eitbuf + i, eitbuf[i + 1] + 2);
					proginfo->audioComponentLen += (eitbuf[i + 1] + 2);
					i += (eitbuf[i + 1] + 2);
					break;
				default:
					i += (eitbuf[i + 1] + 2);
			}
		}

	}

	return;
}


void parseSdt(const unsigned char *sdtbuf, ProgInfoDescriptor *proginfo)
{
	int		totallen = getSectionLength(sdtbuf);

	int		i = 0x0B;

	while(i < totallen - 1)
	{
		int		serviceid = sdtbuf[i] * 256 + sdtbuf[i + 1];
		int		secondlen = getLength(sdtbuf+ i + 3);

		if(serviceid == proginfo->serviceID) {
			i += 5;
			int		j = i;
			while( i < secondlen + j)
			{
				switch(sdtbuf[i])
				{
					case 0x48:													// サービス記述子
						proginfo->serviceLen = sdtbuf[i + 1] + 2;
						memcpy(proginfo->service, sdtbuf + i, proginfo->serviceLen);
						i += (sdtbuf[i + 1] + 2);
						break;
					default:
						i += (sdtbuf[i + 1] + 2);
				}
			}
			break;
		} else {
			i += (secondlen + 5);
		}
	}

	return;
}


void parseNit(const unsigned char *nitbuf, ProgInfoDescriptor *proginfo)
{
	int		totallen	= getSectionLength(nitbuf);
	int		networkid	= nitbuf[0x03] * 256 + nitbuf[0x04];

	int		network_descriptors_length = getLength(nitbuf + 8);

	int		i = 0x0A;
	int		j = i;
	while(i < j + network_descriptors_length) i += (nitbuf[i + 1] + 2);

	int		transport_stream_loop_length = getLength(nitbuf + i);

	i += 2;
	j = i;
	while(i < j + transport_stream_loop_length)
	{
		int		ts_id		= nitbuf[i + 0] * 256 + nitbuf[i + 1];
		int		network_id	= nitbuf[i + 2] * 256 + nitbuf[i + 3];
		i += 4;

		int		transport_descriptors_length = getLength(nitbuf + i);
		i += 2;

		if(ts_id == proginfo->tsID)
		{
			int		k = i;
			while(i < k + transport_descriptors_length)
			{
				switch(nitbuf[i])
				{
					case 0xCD:
						proginfo->tsInformationLen = nitbuf[i + 1] + 2;
						memcpy(proginfo->tsInformation, nitbuf + i, proginfo->tsInformationLen);
						i += (nitbuf[i + 1] + 2);
						break;
					default:
						i += (nitbuf[i + 1] + 2);
						break;
				}
			}
		}
		else
		{
			i += transport_descriptors_length;
		}

	}
	
	return;
}


void parseBit(const unsigned char *bitbuf, ProgInfoDescriptor *proginfo)
{
	int		totallen	= getSectionLength(bitbuf);
	int		networkid	= bitbuf[0x03] * 256 + bitbuf[0x04];

	int		first_descriptors_length = getLength(bitbuf + 8);

	int		i = 0x0A;
	int		j = i;
	while(i < j + first_descriptors_length) i += (bitbuf[i + 1] + 2);

	while(i < totallen - 1)
	{
		int		broadcaster_id = bitbuf[i];
		int		broadcaster_descriptors_length = getLength(bitbuf + i + 1);
		i += 3;

		int		k = i;
		while(i < k + broadcaster_descriptors_length)
		{
				switch(bitbuf[i])
				{
					case 0xCE:
						proginfo->extendedBroadcasterLen = bitbuf[i + 1] + 2;
						memcpy(proginfo->extendedBroadcaster, bitbuf + i, proginfo->extendedBroadcasterLen);
						i += (bitbuf[i + 1] + 2);
						break;
					default:
						i += (bitbuf[i + 1] + 2);
						break;
				};
		}

	}

	return;
}


int rplsTsCheck(HANDLE hReadFile)
{
	DWORD			dwNumRead;
	
	unsigned char	buf[1024];
	memset(buf, 0, 1024);

	SetFilePointer(hReadFile, 0, NULL, FILE_BEGIN);
	ReadFile(hReadFile, buf, 1024, &dwNumRead, NULL);

	if( (buf[0] == 'P') && (buf[1] == 'L') && (buf[2] == 'S') && (buf[3] == 'T') ){														// rpls file
		return	FILE_RPLS;
	}
	else if( (buf[188 * 0] == 0x47) && (buf[188 * 1] == 0x47) && (buf[188 * 2] == 0x47) && (buf[188 * 3] == 0x47) ){					// 188 byte packet ts
		return	FILE_188TS;
	}
	else if( (buf[192 * 0 + 4] == 0x47) && (buf[192 * 1 + 4] == 0x47) && (buf[192 * 2 + 4] == 0x47) && (buf[192 * 3 + 4] == 0x47) ){	// 192 byte packet ts
		return	FILE_192TS;
	}

	return	FILE_INVALID;																												// unknown file
}


BOOL isTB(ProgInfoDescriptor *proginfo)
{
	if( (proginfo->networkIdentifierLen != 0) && (proginfo->networkIdentifier[5] == 'T') && (proginfo->networkIdentifier[6] == 'B') ) return TRUE;

	return FALSE;
}


BOOL readFileProgInfo(_TCHAR *fname, ProgInfoDescriptor* proginfo, const int fpos, const int packet_limit, const CopyOptions *opt)
{
	HANDLE	hFile = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	
	if(hFile == INVALID_HANDLE_VALUE) {
		_tprintf(_T("番組情報元ファイル %s を開くのに失敗しました.\n"), fname);
		return	FALSE;
	}

	int		srcfiletype = rplsTsCheck(hFile);

	if( (srcfiletype != FILE_188TS) &&  (srcfiletype != FILE_192TS) ) {																	// 無効なファイルの場合
		_tprintf(_T("番組情報元ファイル %s は有効なTSファイルではありません.\n"), fname);
		CloseHandle(hFile);
		return	FALSE;
	}

	// 番組情報の読み込み

	int		max_packet_count = packet_limit * 5600;

	BOOL bResult = readTsProgInfo(hFile, proginfo, srcfiletype, fpos, max_packet_count, opt);

	if(!bResult) {
		_tprintf(_T("番組情報元ファイル %s から有効な番組情報を検出できませんでした.\n"), fname);
		CloseHandle(hFile);
		return	FALSE;
	}

	CloseHandle(hFile);

//	show_descriptor(proginfo);

	return	TRUE;
}


BOOL readTsProgInfo(HANDLE hFile, ProgInfoDescriptor *proginfo, const int psize, const int startpos, const int max_packet_count, const CopyOptions *opt)
{
	unsigned char	psibuf[PSIBUFSIZE];
	memset(psibuf, 0, PSIBUFSIZE);

	int	tstype = getSitEit(hFile, psibuf, psize, startpos, &proginfo->tsID, max_packet_count);

	if(tstype == PID_INVALID) return FALSE;

	if(tstype == PID_SIT) {
		parseSit(psibuf, proginfo);
	} else {
		parseEit(psibuf, proginfo);

		unsigned char	sdtbuf[PSIBUFSIZE];
		unsigned char	nitbuf[PSIBUFSIZE];
		unsigned char	bitbuf[PSIBUFSIZE];
		memset(sdtbuf, 0, PSIBUFSIZE);
		memset(nitbuf, 0, PSIBUFSIZE);
		memset(bitbuf, 0, PSIBUFSIZE);

		BOOL	bSDT = TRUE;
		BOOL	bNIT = TRUE;
		BOOL	bBIT = TRUE;

		if(opt->bChannelName) {
			bSDT = FALSE;											// SDTの取得が必要
		}
		if( opt->bChannelNum && isTB(proginfo) ) {
			bNIT = FALSE;											// NITの取得が必要
			bBIT = FALSE;											// BITの取得が必要
		}

		if( !bSDT || !bNIT || !bBIT ) getSdtNitBit(hFile, sdtbuf, &bSDT, nitbuf, &bNIT, bitbuf, &bBIT, psize, proginfo->serviceID, startpos, max_packet_count);

		if(opt->bChannelName) {
			if(bSDT) {
				parseSdt(sdtbuf, proginfo);
			} else {
				_tprintf(_T("有効なSDTパケットを検出できませんでした.サービス記述子はコピーされません.\n"));
			}
		}
		if( opt->bChannelNum && isTB(proginfo) ) {
			if(bNIT) {
				parseNit(nitbuf, proginfo);
			} else {
				_tprintf(_T("有効なNITパケットを検出できませんでした.TS情報記述子はコピーされません.\n"));
			}
			if(bBIT) {
				parseBit(bitbuf, proginfo);
			} else {
				_tprintf(_T("有効なBITパケットを検出できませんでした.拡張ブロードキャスタ記述子はコピーされません.\n"));
			}
		}
	}

	return TRUE;
}


void show_descriptor(const ProgInfoDescriptor *proginfo)
{
	_tprintf(_T("TS ID     0x%.4X\n"), proginfo->tsID);
	_tprintf(_T("ServiceID 0x%.4X\n"), proginfo->serviceID);
	
	_tprintf(_T("\nNetworkIdentifierDescriptor %d\n"), proginfo->networkIdentifierLen);
	disp_code(proginfo->networkIdentifier, proginfo->networkIdentifierLen);

	_tprintf(_T("\nPartialTsTimeDescriptor %d\n"), proginfo->partialTsTimeLen);
	disp_code(proginfo->partialTsTime, proginfo->partialTsTimeLen);

	_tprintf(_T("\nContentDescriptor %d\n"), proginfo->contentLen);
	disp_code(proginfo->content, proginfo->contentLen);

	_tprintf(_T("\nServiceDescriptor %d\n"), proginfo->serviceLen);
	disp_code(proginfo->service, proginfo->serviceLen);

	_tprintf(_T("\nShortEventDescriptor %d\n"), proginfo->shortEventLen);
	disp_code(proginfo->shortEvent, proginfo->shortEventLen);

	_tprintf(_T("\nExtendedEventDescriptor %d\n"), proginfo->extendedEventLen);
	disp_code(proginfo->extendedEvent, proginfo->extendedEventLen);

	_tprintf(_T("\nTsInformationDescriptor %d\n"), proginfo->tsInformationLen);
	disp_code(proginfo->tsInformation, proginfo->tsInformationLen);

	_tprintf(_T("\nExtendedBroadcasterDescriptor %d\n"), proginfo->extendedBroadcasterLen);
	disp_code(proginfo->extendedBroadcaster, proginfo->extendedBroadcasterLen);

	_tprintf(_T("\nEventGroupDescriptor %d\n"), proginfo->eventGroupLen);
	disp_code(proginfo->eventGroup, proginfo->eventGroupLen);

	_tprintf(_T("\nComponentDescriptor %d\n"), proginfo->componentLen);
	disp_code(proginfo->component, proginfo->componentLen);

	_tprintf(_T("\nAudioComponentDescriptor %d\n"), proginfo->audioComponentLen);
	disp_code(proginfo->audioComponent, proginfo->audioComponentLen);

	return;
}


void disp_code(const unsigned char *buf, const int len)
{
	int		i = 0;
	int		j = 0;

	while(i < len)
	{
		_tprintf(_T("%.2X "), buf[i]);
		j++;
		if(j == 16){
			j = 0;
			_tprintf(_T("\n"));
		}
		i++;
	}
	_tprintf(_T("\n"));
}

