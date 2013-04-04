#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

#include "pmt.h"
#include "tsprocess.h"

//
//
// PMT処理クラス
// PmtProcessのメンバ定義

PmtProcess::PmtProcess(const int numpid, const int serviceid, const int uniqueid)
{
	pid = numpid;
	uid = uniqueid;

	service_id	= serviceid;

	bPmtStarted = FALSE;

	memset(buf, 0xFF, MAX_PMT_SIZE);
	memset(pmt, 0xFF, MAX_PMT_SIZE);
	memset(conv, 0xFF, MAX_PMT_SIZE);

	buflen = 0;
	adaplen = 0;
	seclen = 0;

	pmtlen = 0;
	convlen = 0;

	wcount = 0;

	pmtnum = 0;
	pmtcount = 0;

	lifecount = 0;
}

// PmtProcess::~PmtProcess()
// {
// }

int PmtProcess::getPacket(const unsigned char *packet, unsigned char *wbuf)
{

	if(getPid(packet) != pid) {											// PMTパケットでは無かった場合

		if(!bPmtStarted) return PMT_NOT_PMT_PACKET;

		lifecount++;

		if(lifecount > MAX_PMT_PACKET_LIFE) {
			bPmtStarted = FALSE;
			lifecount = 0;
			return PMT_LIFE_EXPIRED;
		}

		return PMT_NOT_PMT_PACKET;
	}


	// 受け取ったPMTパケットを処理

	if(isPsiTop(packet))												// PMTパケット先頭の場合
	{
		wcount = 0;
		waddr[wcount++] = wbuf;

		memcpy(buf, packet + 4, 184);
		buflen = 184;

		adaplen = getAdapFieldLength(packet);
		seclen  = getSectionLength(packet + adaplen + 5);

		bPmtStarted = TRUE;
//		lifecount = 0;

		if( buflen < (adaplen + seclen + 4) ) return PMT_STARTED;		// このパケットにまだ続きがあるとき
	}
	else																// PMTパケット非先頭の場合
	{
		if(!bPmtStarted) return PMT_NOT_STARTED;						// PMTがSTARTしていないのに非先頭が来たら

		waddr[wcount++] = wbuf;
		memcpy(buf + buflen, packet + 4, 184);

		buflen += 184;

		if( buflen < (adaplen + seclen + 4) ) return PMT_CONTINUED;		// このパケットにまだ続きがあるとき
	}


	// パケットが揃ったらCRCチェック

	if( !(this->isCrcOk()) ) {											// CRCが合わない場合

		bPmtStarted = FALSE;
		lifecount = 0;

		if(wcount == 1) {
			return PMT_SINGLE_PACKET_CRC_ERROR;
		} else {
			return PMT_MULTI_PACKET_CRC_ERROR;
		}
	}


	// 先行するPMTと比較, 新たなPMTなら入れ替え

	if(this->isNewPmt()) {
//		printf("new pmt found. %.2X\n", (buf[adaplen + 0x06] & 0x3E) >> 1);
		this->copyPmt();
		this->convPmt();
	}

	
	// 変換済みPMTを出力

	int		i;

	for(i = 0; i < wcount; i++) {

		memcpy(waddr[i] + 4, conv + 184 * pmtcount, 184);

		if(pmtcount == 0) {
			*(waddr[i] + 1) |= 0x40;									// payload_unit_start_indicatorをセットする
		} else {
			*(waddr[i] + 1) &= 0xBF;									// payload_unit_start_indicatorをリセットする
		}

		pmtcount++;
		if(pmtcount == pmtnum) pmtcount = 0;
	}

	bPmtStarted = FALSE;
	lifecount = 0;

	if(wcount == 1) return PMT_SINGLE_PACKET_CONVED;

	return PMT_MULTI_PACKET_CONVED;
}

BOOL PmtProcess::isCrcOk()
{
	unsigned int	crc = calc_crc32(buf + adaplen + 1, seclen + 3);

	if(crc == 0) return TRUE;

	return FALSE;
}

BOOL PmtProcess::isNewPmt()
{
	if( pmtlen != (adaplen + seclen + 4) ) return TRUE;

	int result = memcmp(pmt, buf, pmtlen);

	if(result == 0) return FALSE;

	return TRUE;
}

void PmtProcess::copyPmt()
{
	pmtlen = adaplen + seclen + 4;
	memcpy(pmt, buf, pmtlen);
	memcpy(conv, buf, pmtlen);

	return;
}

void PmtProcess::convPmt()
{
	unsigned char	*dat = conv + adaplen;

	int		section_length = getSectionLength(dat + 1);

	dat[0x04] = service_id >> 8;									// serviceID (program_number) 書き換え
	dat[0x05] = service_id & 0x00FF;


	// CRC再計算

	unsigned int	crc = calc_crc32(dat + 1, section_length - 1);

	dat[section_length + 0] = (crc >> 24) & 0xFF;
	dat[section_length + 1] = (crc >> 16) & 0xFF;
	dat[section_length + 2] = (crc >>  8) & 0xFF;
	dat[section_length + 3] = (crc >>  0) & 0xFF;

	convlen = section_length + adaplen + 4;							// conv全体のデータ量

	memset(conv + convlen, 0xFF, MAX_PMT_SIZE - convlen);			// データの後ろは0xFFで埋める

	pmtnum = convlen / 184;											// データが何パケット分か計算
	if( (convlen % 184) != 0 ) pmtnum++;

	pmtcount = 0;													// 出力用カウントを0に戻す

	return;
}

