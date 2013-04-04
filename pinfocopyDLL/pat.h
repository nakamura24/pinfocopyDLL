#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

// 定数

#define		MAX_PAT_SIZE					1288			// 7パケット分

#define		PAT_NOT_PAT_PACKET				0
#define		PAT_NOT_STARTED					1
#define		PAT_STARTED						2
#define		PAT_CONTINUED					3
#define		PAT_SINGLE_PACKET_CRC_ERROR		4
#define		PAT_MULTI_PACKET_CRC_ERROR		5
#define		PAT_SINGLE_PACKET_CONVED		6
#define		PAT_MULTI_PACKET_CONVED			7
#define		PAT_LIFE_EXPIRED				8

#define		MAX_PAT_PACKET_LIFE				16000


// PAT処理クラス

class	PatProcess
{
public:
	int			pid;
	int			uid;

	PatProcess(const int numpid, const int tsid, const int serviceid, const int uniqueid);
// 	~PatProcess();

	int				getPacket(const unsigned char *packet, unsigned char *wbuf);

private:
	unsigned char	buf[MAX_PAT_SIZE];	// 今現在PATパケットを取り込んでいるバッファ
	int				buflen;				// そのデータのトータルの長さ
	int				adaplen;			// アダプテーションフィールド長
	int				seclen;				// セクション長

	unsigned char	*waddr[MAX_PAT_SIZE / 184];		// 変換パケット書き出しの為のアドレス記録
	int				wcount;							// その個数

	unsigned char	pat[MAX_PAT_SIZE];	// 先行するPATのバッファ
	int				patlen;				// そのデータのトータルの長さ

	unsigned char	conv[MAX_PAT_SIZE];	// 先行するPATの変換後バッファ
	int				convlen;			// そのデータのトータルの長さ
	int				patnum;				// そのデータが何パケット分か
	int				patcount;			// 出力時のカウント用

	BOOL			bPatStarted;		// 複数パケットにまたがるPATが開始したらTRUEになる
	int				lifecount;			// 複数パケットのPATが開始してからのパケットカウント用
										// カウントがMAX_PAT_PACKET_LIFEを超えたらそのPATは無効にする(overflow防止用)

	int				ts_id;				// 書き換え用 transport_stream_ID
	int				service_id;			// 書き換え用 service_ID (program_number)

	BOOL			isCrcOk();			// bufのCRCチェック
	BOOL			isNewPat();			// bufとpatの比較
	void			copyPat();			// bufからpat, convへのコピー
	void			convPat();			// convにコピーしたPATの変換
};