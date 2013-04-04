#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

#include "sitprocess.h"
#include "tsprocess.h"
#include "proginfo.h"

//
//
// SIT処理クラス
// SitProcessのメンバ定義

SitProcess::SitProcess(const int npid, const int npcrpid, const CopyOptions *opt)
{
	pid			= npid;
	pcrpid		= npcrpid;
	
	bChNum			= opt->bChannelNum;
	bChName			= opt->bChannelName;
	bProgTime		= opt->bProgTime;
	bProgName		= opt->bProgName;
	bProgDetail		= opt->bProgDetail;
	bProgExt		= opt->bProgExtend;
	bProgGenre		= opt->bProgGenre;
	bVComponent		= opt->bVComponent;
	bAComponent		= opt->bAComponent;
	numAComponent	= opt->numAComponent;

	memset(sitbuf, 0, sizeof(sitbuf));
	
	buflen		= 0;
	sitnum		= 0;
	sitcount	= 0;

	ptst_desc_pos	= -1;
	pcr_counter		= 0;
	prev_pcr		= 0;
	bPcrStarted		= FALSE;

	time_mjd		= 0;
	time_sec		= 0;
	time_sec_old	= 0;

	version_number	= 0;
}

// SitProcess::~SitProcess()
// {
// }

int SitProcess::makeNewSit(const unsigned char *psibuf, const ProgInfoDescriptor *proginfo)
{
	// オリジナルのSITと提供される番組情報から、新たなSITを作成する

	int		src	= 0;
	int		dst	= 0;

	sitbuf[0x00] = 0x00;											//　pointerfield 0x00

	memcpy(sitbuf + 1, psibuf, 10);									//　TableID 0x7F ～ Transmission_info_loop_length まで

	int		section_length					= getSectionLength(psibuf);
	int		transmission_info_loop_length	= getLength(psibuf + 0x08);

	sitbuf[0x06] |= 0x01;											// カレントネクスト指示 "1"
	sitbuf[0x07] = 0;												// セクション番号 0
	sitbuf[0x08] = 0;												// 最終セクション番号 0

	src += 10;
	dst += 11;


	// 伝送情報に関する記述子ループ

	int		new_transmission_info_loop_length = 0;

	for(int i = 0; i < transmission_info_loop_length; )
	{
		int	tag	= psibuf[src];
		int	len	= psibuf[src + 1] + 2;

		if( ((tag == 0xC2) && bChNum) ||							// bChNum指定なら元からあったネットワーク識別記述子は消す
			((tag == 0xCD) && bChNum) ||							// bChNum指定なら元からあったTS情報記述子は消す
			((tag == 0xC3) && bProgTime) )							// bProgTime指定なら元からあったパーシャルTSタイム記述子は消す
		{
			i	+= len;
			src += len;
		}
		else														// 上記以外の記述子は残す
		{
			memcpy(sitbuf + dst, psibuf + src, len);

			if( (tag == 0xC3) && ( (sitbuf[dst + 0x0E] & 0x01) != 0) )	ptst_desc_pos = dst;		// 現在時刻を有するパーシャルTSタイム記述子を残す場合

			i	+= len;
			src += len;
			dst += len;
			new_transmission_info_loop_length += len;
		}
	}


	// 記述子の追加

	if(bChNum) {
		memcpy(sitbuf + dst, proginfo->networkIdentifier, proginfo->networkIdentifierLen);			// 新たなネットワーク識別記述子を追加
		dst += proginfo->networkIdentifierLen;
		new_transmission_info_loop_length += proginfo->networkIdentifierLen;
	}

	if(bChNum) {
		memcpy(sitbuf + dst, proginfo->tsInformation, proginfo->tsInformationLen);					// 新たなTS情報記述子を追加
		dst += proginfo->tsInformationLen;
		new_transmission_info_loop_length += proginfo->tsInformationLen;
	}


	if(new_transmission_info_loop_length >= 4096) return SIT_TRANSMISSION_INFO_LOOP_TOO_LONG;		// 新たに作成したtransmission_info_loopが長すぎる場合

	sitbuf[0x09] = (new_transmission_info_loop_length | 0xF000) >> 8;								// 新たなtransmission_info_loop_lengthに更新
	sitbuf[0x0A] = new_transmission_info_loop_length & 0x00FF;


	//serviceごとのループ（SITでは通常は一つのサービスだけ？）

	while( src < (section_length - 1) )
	{
		memcpy(sitbuf + dst, psibuf + src, 4);														// サービスID ～ サービスループ長までコピー

		int		service_loop_length = getLength(psibuf + src + 2);

		if(bChNum) {
			sitbuf[dst + 0] = proginfo->serviceID >> 8;												// service_idの変更
			sitbuf[dst + 1] = proginfo->serviceID & 0xFF;
		}

		src += 4;
		dst += 4;


		// 各サービスに関する記述子ループ

		unsigned char	oldShortEvent[MAXDESCSIZE];
		memset(oldShortEvent, 0, sizeof(oldShortEvent));
		int		oldShortEventLen = 0;

		int		new_service_loop_length = 0;

		for(int i = 0; i < service_loop_length; )
		{
			int	tag	= psibuf[src];
			int	len	= psibuf[src + 1] + 2;

			if( ((tag == 0xD6) && bChNum) ||						// bChNum指定なら元のイベントグループ記述子は消す
				((tag == 0x85) && bChNum) ||						// bChNum指定なら元の放送ID記述子は消す
				((tag == 0xD8) && bChNum) ||						// bChNum指定なら元のブロードキャスタ名記述子は消す
				((tag == 0xCE) && bChNum) ||						// bChNum指定なら元の拡張ブロードキャスタ記述子は消す
				((tag == 0xC3) && bProgTime) ||						// bProgTime指定なら元のパーシャルTSタイム記述子は消す
				((tag == 0x48) && bChName) ||						// bChName指定なら元のサービス記述子は消す
				((tag == 0x4D) && (bProgName || bProgDetail)) ||	// bProgNameもしくはbProgDetail指定なら元の短形式イベント記述子は消す
				((tag == 0x4E) && bProgExt) ||						// bProgExt指定なら元の拡張形式イベント記述子は消す
				((tag == 0x54) && bProgGenre) ||					// bProgGenre指定なら元のコンテント記述子は消す
				((tag == 0x50) && bVComponent) ||					// bVComponent指定なら元のコンポーネント記述子は消す
				((tag == 0xC4) && bAComponent) )					// bAComponent指定なら元の音声コンポーネント記述子は消す
			{
				if( (tag == 0x4D) && (bProgName ^ bProgDetail) )	// 短形式イベント記述子を消す場合であっても ProgName/ProgDetail のどちらか一方を残す必要がある場合
				{
					memcpy(oldShortEvent, psibuf + src, len);		// 元の短形式イベント記述子を保存しておく
					oldShortEventLen = len;
				}

				i	+= len;
				src += len;
			}
			else													// 上記以外の記述子は残す
			{
				memcpy(sitbuf + dst, psibuf + src, len);

				if(tag == 0xC3)										// パーシャルTSタイム記述子を残す場合、番組の開始時刻を保存しておく
				{
					time_mjd = sitbuf[dst + 0x03] * 256 + sitbuf[dst + 0x04];
					time_sec =	(sitbuf[dst + 0x05] >> 4) * 36000 + (sitbuf[dst + 0x05] & 0x0F) * 3600 +
								(sitbuf[dst + 0x06] >> 4) * 600 + (sitbuf[dst + 0x06] & 0x0F) * 60 +
								(sitbuf[dst + 0x07] >> 4) * 10 + (sitbuf[dst + 0x07] & 0x0F);
					time_sec_old = time_sec;
					if( (sitbuf[dst + 0x0E] & 0x01) != 0 ) ptst_desc_pos = dst;
				}

				i	+= len;
				src += len;
				dst += len;
				new_service_loop_length += len;
			}
		}


		// 記述子の追加

		if( bProgTime && (proginfo->partialTsTimeLen != 0) ) {
			memcpy(sitbuf + dst, proginfo->partialTsTime, proginfo->partialTsTimeLen);				// 新たなパーシャルTSタイム記述子の追加
			ptst_desc_pos = dst;
			dst += proginfo->partialTsTimeLen;
			new_service_loop_length += proginfo->partialTsTimeLen;

			time_mjd = proginfo->partialTsTime[0x03] * 256 + proginfo->partialTsTime[0x04];
			time_sec =	(proginfo->partialTsTime[0x05] >> 4) * 36000 + (proginfo->partialTsTime[0x05] & 0x0F) * 3600 +
						(proginfo->partialTsTime[0x06] >> 4) * 600 + (proginfo->partialTsTime[0x06] & 0x0F) * 60 +
						(proginfo->partialTsTime[0x07] >> 4) * 10 + (proginfo->partialTsTime[0x07] & 0x0F);
			time_sec_old = time_sec;
		}

		if(bVComponent) {
			memcpy(sitbuf + dst, proginfo->component, proginfo->componentLen);						// 新たなコンポーネント記述子の追加
			dst += proginfo->componentLen;
			new_service_loop_length += proginfo->componentLen;
		}

		if(bAComponent) {
			int	count = 1;
			for(int i = 0; i < proginfo->audioComponentLen; )
			{
				int len = proginfo->audioComponent[i + 1] + 2;
				if( (count == numAComponent) || (numAComponent == 0) ) {
					int len = proginfo->audioComponent[i + 1] + 2;
					memcpy(sitbuf + dst, proginfo->audioComponent + i, len);						// 新たな音声コンポーネント記述子の追加
					dst += len;
					new_service_loop_length += len;
				}
				i += len;
				count++;
			}
		}

		if(bChNum) {
			memcpy(sitbuf + dst, proginfo->extendedBroadcaster, proginfo->extendedBroadcasterLen);		// 新たな拡張ブロードキャスタ記述子の追加
			dst += proginfo->extendedBroadcasterLen;
			new_service_loop_length += proginfo->extendedBroadcasterLen;
		}

		if(bChNum) {
			memcpy(sitbuf + dst, proginfo->broadcasterName, proginfo->broadcasterNameLen);			// 新たなブロードキャスタ名記述子の追加
			dst += proginfo->broadcasterNameLen;
			new_service_loop_length += proginfo->broadcasterNameLen;
		}

		if(bChNum) {
			memcpy(sitbuf + dst, proginfo->eventGroup, proginfo->eventGroupLen);					// 新たなイベントグループ記述子の追加
			dst += proginfo->eventGroupLen;
			new_service_loop_length += proginfo->eventGroupLen;
		}

		if(bChName) {
			memcpy(sitbuf + dst, proginfo->service, proginfo->serviceLen);							// 新たなサービス記述子の追加
			dst += proginfo->serviceLen;
			new_service_loop_length += proginfo->serviceLen;
		}

		if(bProgName && bProgDetail) {
			memcpy(sitbuf + dst, proginfo->shortEvent, proginfo->shortEventLen);					// 新たな短形式イベント記述子の追加
			dst += proginfo->shortEventLen;
			new_service_loop_length += proginfo->shortEventLen;
		}

		if(bProgName ^ bProgDetail) {																// 元の短形式イベント記述子と新たな短形式イベント記述子を一緒にして作り直し、それを追加する
			const unsigned char		*newShortEvent = proginfo->shortEvent;
			const int				newShortEventLen = proginfo->shortEventLen; 

			int		old_pname_len	= (oldShortEventLen == 0) ? 0 : oldShortEvent[5];
			int		old_pdetail_len	= (oldShortEventLen == 0) ? 0 : oldShortEvent[6 + old_pname_len];
			int		new_pname_len	= (newShortEventLen == 0) ? 0 : newShortEvent[5];
			int		new_pdetail_len	= (newShortEventLen == 0) ? 0 : newShortEvent[6 + new_pname_len];

			unsigned char	tmpDescriptor[MAXDESCSIZE];
			int				tmpDescriptorLen	= 0;
			memset(tmpDescriptor, 0, sizeof(tmpDescriptor));
			tmpDescriptor[0] = 0x4D;
			tmpDescriptor[2] = 0x6A;
			tmpDescriptor[3] = 0x70;
			tmpDescriptor[4] = 0x6E;
			
			if(bProgName) {
				if( (new_pname_len + old_pdetail_len > 240) && (new_pname_len > 80) ) new_pname_len = 80;
				if( (new_pname_len + old_pdetail_len > 240) && (old_pdetail_len > 160) ) old_pdetail_len = 160;
				tmpDescriptor[5] = (unsigned char)new_pname_len;
				memcpy(tmpDescriptor + 6, newShortEvent + 6, new_pname_len);
				tmpDescriptor[6 + new_pname_len] = (unsigned char)old_pdetail_len;
				memcpy(tmpDescriptor + 7 + new_pname_len, oldShortEvent + 7 + old_pname_len, old_pdetail_len);
				tmpDescriptorLen = 5 + new_pname_len + old_pdetail_len;
			}

			if(bProgDetail) {
				if( (old_pname_len + new_pdetail_len > 240) && (old_pname_len > 80) ) old_pname_len = 80;
				if( (old_pname_len + new_pdetail_len > 240) && (new_pdetail_len > 160) ) new_pdetail_len = 160;
				tmpDescriptor[5] = (unsigned char)old_pname_len;
				memcpy(tmpDescriptor + 6, oldShortEvent + 6, old_pname_len);
				tmpDescriptor[6 + old_pname_len] = (unsigned char)new_pdetail_len;
				memcpy(tmpDescriptor + 7 + old_pname_len, newShortEvent + 7 + new_pname_len, new_pdetail_len);
				tmpDescriptorLen = 5 + old_pname_len + new_pdetail_len;
			}

			tmpDescriptor[1] = (unsigned char)tmpDescriptorLen;
			tmpDescriptorLen += 2;

			if(tmpDescriptorLen == 7) tmpDescriptorLen = 0;											// 中身が空の短形式イベント記述子になった場合は存在しないものとしてコピーしない

			memcpy(sitbuf + dst, tmpDescriptor, tmpDescriptorLen);									// 新たに作成し直した短形式イベント記述子の追加
			dst += tmpDescriptorLen;
			new_service_loop_length += tmpDescriptorLen;
		}

		if(bProgExt) {
			memcpy(sitbuf + dst, proginfo->extendedEvent, proginfo->extendedEventLen);				// 新たな拡張形式イベント記述子の追加
			dst += proginfo->extendedEventLen;
			new_service_loop_length += proginfo->extendedEventLen;
		}

		if(bProgGenre) {
			memcpy(sitbuf + dst, proginfo->content, proginfo->contentLen);							// 新たなコンテント記述子の追加
			dst += proginfo->contentLen;
			new_service_loop_length += proginfo->contentLen;
		}

		if(new_service_loop_length > 4096) return SIT_SERVICE_LOOP_TOO_LONG;

		sitbuf[dst - new_service_loop_length - 2] = (new_service_loop_length | 0x8000) >> 8;		// 新たなservice_loop_lengthに更新
		sitbuf[dst - new_service_loop_length - 1] = new_service_loop_length & 0xFF;
	
	}

	if(dst > 4093) return SIT_SECTION_TOO_LONG;														// section長 > 4093 なら不適

	sitbuf[0x02] &= 0xF0;																			// 新たなsection_lengthに更新
	sitbuf[0x02] |= (dst >> 8);
	sitbuf[0x03] = dst & 0xFF;


	// CRC再計算

	unsigned int	crc = calc_crc32(sitbuf + 1, dst - 1);

	sitbuf[dst + 0] = (crc >> 24) & 0xFF;
	sitbuf[dst + 1] = (crc >> 16) & 0xFF;
	sitbuf[dst + 2] = (crc >>  8) & 0xFF;
	sitbuf[dst + 3] = (crc >>  0) & 0xFF;

	memset(sitbuf + dst + 4, 0xFF, sizeof(sitbuf) - dst - 4);										// 残りは0xFFで埋める


	// その他

	buflen	= dst + 4;

	sitnum = buflen / 184;																			// データが何パケット分か計算
	if( (buflen % 184) != 0 ) sitnum++;


	return SIT_MAKE_NEW_SIT_SUCCESS;
}

	
int SitProcess::getPacket(const unsigned char *packet, unsigned char *wbuf)
{

	// PCRからの経過時間情報の取得，ptst_desc_pos != -1 なら書き換えるべきパーシャルTSタイム記述子が存在する

	if( (ptst_desc_pos != -1) && (getPid(packet) == pcrpid) && isPcrData(packet) )
	{
		__int64		new_pcr = getPcrValue(packet);																		// PCR値取得

		if(!bPcrStarted) {
			prev_pcr = new_pcr;																							// 最初のPCR値の場合
			bPcrStarted = TRUE;
		}

		__int64		pcr_interval = (new_pcr >= prev_pcr) ? new_pcr - prev_pcr : new_pcr + 2576980377600 - prev_pcr;		// PCR値の間隔（経過時間）の計算
		if(pcr_interval > SIT_MAX_PCR_TIME_INTERVAL) pcr_interval = SIT_MAX_PCR_TIME_INTERVAL;							// 何らかの理由でPCR値の間隔が広すぎる場合、一定値に抑える

		pcr_counter += pcr_interval;																					// トータルの経過時間カウンタ
		prev_pcr = new_pcr;
	}


	// SITパケットに関する置換処理

	if(getPid(packet) != pid) return SIT_NOT_SIT_PACKET;											// SITパケットではなかった場合


	// パーシャルTSタイム記述子の現在時刻修正

	if( (ptst_desc_pos != -1) && (sitcount == 0) )
	{
		// 現在時刻計算．ファイルの先頭を番組開始時刻とし、その後はPCRに従って時間経過

		int	sec		= time_sec + (int)(pcr_counter / 27000000);

		int	newmjd	= time_mjd + (sec / 86400);
		int	newhour	= (sec % 86400) / 3600;
		int	newmin	= (sec % 3600) / 60;
		int	newsec	= sec % 60;

		// 時刻書き換え

		sitbuf[ptst_desc_pos + 0x0F] = newmjd >> 8;
		sitbuf[ptst_desc_pos + 0x10] = newmjd & 0x00FF;
		sitbuf[ptst_desc_pos + 0x11] = (newhour / 10) * 16 + (newhour % 10);
		sitbuf[ptst_desc_pos + 0x12] = (newmin / 10) * 16 + (newmin % 10);
		sitbuf[ptst_desc_pos + 0x13] = (newsec / 10) * 16 + (newsec % 10);

		if(sec != time_sec_old) version_number++;												// 時刻情報が更新されたらSITのバージョン番号を上げる
		if(version_number == 32) version_number = 0;
		time_sec_old = sec;
		sitbuf[0x06] = (version_number << 1) | 0xC1;											// バージョン番号書き込み

		// CRC再計算して修正

		unsigned int	crc = calc_crc32(sitbuf + 1, buflen - 5);

		sitbuf[buflen - 4] = (crc >> 24) & 0xFF;
		sitbuf[buflen - 3] = (crc >> 16) & 0xFF;
		sitbuf[buflen - 2] = (crc >>  8) & 0xFF;
		sitbuf[buflen - 1] = (crc >>  0) & 0xFF;
	}


	// 変換済みのSITを1パケットごとに出力

	memcpy(wbuf + 4, sitbuf + sitcount * 184, 184);												// 出力バッファにコピー

	if(sitcount == 0) {
		*(wbuf + 1) |= 0x40;																	// payload_unit_start_indicatorをセットする
	} else {
		*(wbuf + 1) &= 0xBF;																	// payload_unit_start_indicatorをリセットする
	}

	*(wbuf + 3) &= 0xDF;																		// adaptation_filed無し
	*(wbuf + 3) |= 0x10;																		// payloadあり

	sitcount++;
	if(sitcount == sitnum) sitcount = 0;

	return SIT_REPLACEMENT_SUCCESS;
}




	





