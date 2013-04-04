#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

// 定数

#define		MAX_PMT_SIZE					1288			// 7パケット分

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


// PMT処理クラス

class	PmtProcess
{
public:
	int			pid;
	int			uid;

	PmtProcess(const int numpid, const int serviceid, const int uniqueid);
// 	~PmtProcess();

	int				getPacket(const unsigned char *packet, unsigned char *wbuf);

private:
	unsigned char	buf[MAX_PMT_SIZE];	// 今現在PMTパケットを取り込んでいるバッファ
	int				buflen;				// そのデータのトータルの長さ
	int				adaplen;			// アダプテーションフィールド長
	int				seclen;				// セクション長

	unsigned char	*waddr[MAX_PMT_SIZE / 184];		// 変換パケット書き出しの為のアドレス記録
	int				wcount;							// その個数

	unsigned char	pmt[MAX_PMT_SIZE];	// 先行するPMTのバッファ
	int				pmtlen;				// そのデータのトータルの長さ

	unsigned char	conv[MAX_PMT_SIZE];	// 先行するPMTの変換後バッファ
	int				convlen;			// そのデータのトータルの長さ
	int				pmtnum;				// そのデータが何パケット分か
	int				pmtcount;			// 出力時のカウント用

	BOOL			bPmtStarted;		// 複数パケットにまたがるPMTが開始したらTRUEになる
	int				lifecount;			// 複数パケットのPMTが開始してからのパケットカウント用
										// カウントがMAX_PMT_PACKET_LIFEを超えたらそのPMTは無効にする(overflow防止用)

	int				service_id;			// 書き換え用 service_ID (program_number)

	BOOL			isCrcOk();			// bufのCRCチェック
	BOOL			isNewPmt();			// bufとpmtの比較
	void			copyPmt();			// bufからpmt, convへのコピー
	void			convPmt();			// convにコピーしたPMTの変換
};