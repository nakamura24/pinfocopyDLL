#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include "convtounicode.h"

#define		IF_NSZ_TO_MSZ		if(bCharSize&&sta.bNormalSize){charbuf(dbuf,maxbufsize,dst++,0x89);sta.bNormalSize=FALSE;}
#define		IF_MSZ_TO_NSZ		if(bCharSize&&!sta.bNormalSize){charbuf(dbuf,maxbufsize,dst++,0x8A);sta.bNormalSize=TRUE;}
#define		WCHARBUF(x)			dst+=wcharbuf(dbuf,maxbufsize,dst,x,sta.bXCS)
#define		CHARBUF(x)			charbuf(dbuf,maxbufsize,dst++,x)

#define		NORMAL_NEWLINE_CODE				TRUE			// TRUE�Ȃ���s�R�[�h��0x0D���g�p, FALSE�Ȃ�0x0A���g�p����

// #define		USE_CONV_FROM_UNICODE3						// conv_from_unicode3()���g�p����


int conv_to_unicode(WCHAR *dbuf, const int maxbufsize, const unsigned char *sbuf, const int total_length, const BOOL bCharSize)
{

//		8�P�ʕ��������� -> UNICODE(UTF-16LE)������ւ̕ϊ�
//
//		sbuf				�ϊ���buf
//		total_length		���̒���(byte�P��, NULL���������܂܂Ȃ�)
//		dbuf				�ϊ���buf
//		maxbufsize			�ϊ���buf�̍ő�T�C�Y(WCHAR�P��), �z�������͏������܂ꂸ���������
//		bCharSize			�X�y�[�X�y�щp�������̕ϊ��ɕ����T�C�Y�w��(NSZ, MSZ)�𔽉f�����邩�ۂ��DTRUE�Ȃ甽�f������
//		�߂�l				�ϊ����Đ�������WCHAR������̒���(WCHAR�P��)
//							dbuf��NULL���w�肷��ƕϊ����Đ�������������̒���(WCHAR�P��)�����Ԃ�

	ConvStatus	sta;
	initConvStatus(&sta);

	int		src = 0;
	int		dst = 0;


	// �ϊ����C��

	while(src < total_length)
	{
		if(isControlChar(sbuf[src]))
		{
			// 0x00�`0x20, 0x7F�`0xA0, 0xFF�̏ꍇ

			switch(sbuf[src])
			{
				case 0x08:				// APB (BS)
					WCHARBUF(0x0008);						// BS�o��
					src++;
					break;
				case 0x09:				// APF (TAB)
					WCHARBUF(0x0009);						// TAB�o��
					src++;
					break;
				case 0x0A:				// APD (LF)
					WCHARBUF(0x000D);						// CR+LF�o��
					WCHARBUF(0x000A);
					src++;
					break;
				case 0x0D:				// APR (CR)
					if(sbuf[src + 1] != 0x0A) {
						WCHARBUF(0x000D);					// CR+LF�o��	
						WCHARBUF(0x000A);
					}
					src++;
					break;
				case 0x20:				// SP
					if( bCharSize && sta.bNormalSize ) {
						WCHARBUF(0x3000);					// �S�pSP�o��
					} else {
						WCHARBUF(0x0020);					// ���pSP�o��
					}
					src++;
					break;
				case 0x7F:				// DEL
					WCHARBUF(0x007F);						// DEL�o��
					src++;
					break;
				case 0x9B:				// CSI����
					csiProc(sbuf, &src, &sta);
					break;
				default:				// ����ȊO�̐���R�[�h
					changeConvStatus(sbuf, &src, &sta);
					break;
			}
		}
		else
		{
			// GL, GR�ɑΉ����镶���o��

			int		jis, uc, len;
			WCHAR	utfstr[UTF16TABLELEN];

			int		regionLR = (sbuf[src] >= 0x80) ? REGION_GR : REGION_GL;

			switch(sta.bank[sta.region[regionLR]])
			{
				case F_KANJI:
				case F_JIS1KANJI:
					jis = (sbuf[src] & 0x7F) * 256 + (sbuf[src + 1] & 0x7F);
					uc = 0;
					if( bCharSize && !sta.bNormalSize ) uc = charsize1conv(jis, FALSE);			// MSZ�w��̉p�������̏ꍇ�͔��p�����ɕϊ�����
					if(uc == 0) uc = jis12conv(jis, TRUE);
					if(uc == 0) uc = jis3conv(jis, TRUE);
					if(uc != 0) {
                        WCHARBUF(uc);
                    } 
                    else if( (len = jis3combconv(jis, utfstr, UTF16TABLELEN)) != 0 ) {
                        for(int i = 0; i < len; i++) WCHARBUF(utfstr[i]);
                    }
					src += 2;
					break;
				case F_JIS2KANJI:
					jis = (sbuf[src] & 0x7F) * 256 + (sbuf[src + 1] & 0x7F);
					uc = jis4conv(jis, TRUE);
					if(uc != 0) WCHARBUF(uc);
					src += 2;
					break;
				case F_ALPHA:
				case F_P_ALPHA:
					jis = sbuf[src] & 0x7F;
					if( bCharSize && sta.bNormalSize ) {
						uc = jis12conv(charsize1conv(jis, TRUE), TRUE);							// NSZ�w��̏ꍇ�͑S�p�����ɕϊ�����
					} else {
						uc = jis;																// tilde��overline���p
					}
					if(uc != 0) WCHARBUF(uc);
					src++;
					break;
				case F_HIRAGANA:
				case F_P_HIRAGANA:
					uc = hiragana1conv(sbuf[src] & 0x7F, TRUE);
					if(uc != 0) WCHARBUF(uc);
					src++;
					break;
				case F_KATAKANA:
				case F_P_KATAKANA:
					uc = katakana1conv(sbuf[src] & 0x7F, TRUE);
					if(uc != 0) WCHARBUF(uc);
					src++;
					break;
				case F_HANKAKU:
					uc = hankaku1conv(sbuf[src] & 0x7F, TRUE);
					if(uc != 0) WCHARBUF(uc);
					src++;
					break;
				case F_TUIKAKIGOU:
					jis = (sbuf[src] & 0x7F) * 256 + (sbuf[src + 1] & 0x7F);
					uc = tuikakigou1conv(jis, TRUE);
					if( (uc == 0) && bCharSize && !sta.bNormalSize ) uc = charsize1conv(jis, TRUE);		// �ǉ��L���W����1�`84��͖���`���Ǝv���̂ł����A�p�i�����R�ł͂��̕�����
					if(uc == 0) uc = jis12conv(jis, TRUE);												// �����n�W���Ɠ���̕������o�͂����Ă���Ⴊ�������̂ł������Ă���܂��i�\�j�[�����R�ɂ����čs���Ɖ����܂��j
					if(uc != 0) {
						WCHARBUF(uc);
					}
                    else if( (len = tuikakigou2conv(jis, utfstr, UTF16TABLELEN)) != 0 ) {
                        for(int i = 0; i < len; i++) WCHARBUF(utfstr[i]);
                    }
					src += 2;
					break;
				case F_MOSAICA:
				case F_MOSAICB:
				case F_MOSAICC:
				case F_MOSAICD:
				case F_DRCS0:
				case F_DRCS1A:
				case F_DRCS2A:
				case F_DRCS3A:
				case F_DRCS4A:
				case F_DRCS5A:
				case F_DRCS6A:
				case F_DRCS7A:
				case F_DRCS8A:
				case F_DRCS9A:
				case F_DRCS10A:
				case F_DRCS11A:
				case F_DRCS12A:
				case F_DRCS13A:
				case F_DRCS14A:
				case F_DRCS15A:
					WCHARBUF(0x003F);						// '?'�o��
					src++;
					break;
				case F_MACROA:
					defaultMacroProc(sbuf[src] & 0x7F, &sta);
					src++;
					break;
				default:
					break;
			}

			if(sta.bSingleShift) {
				sta.region[REGION_GL] = sta.region_GL_backup;
				sta.bSingleShift = FALSE;
			}
		}
	}

	WCHARBUF(0x0000);

	dst--;
	if(dst > maxbufsize) dst = maxbufsize; 

	return dst;			// �ϊ���̒�����Ԃ�(WCHAR�P��), �I�[�̃k�����������܂܂Ȃ�
}


int conv_from_unicode(unsigned char *dbuf, const int maxbufsize, const WCHAR *sbuf, const int total_length, const BOOL bCharSize)
{

//		UNICODE(UTF-16LE)������ -> 8�P�ʕ���������ւ̕ϊ�
//
//		sbuf				�ϊ���buf
//		total_length		���̒���(WCHAR�P��, NULL���������܂܂Ȃ�)
//		dbuf				�ϊ���buf
//		maxbufsize			�ϊ���buf�̍ő�T�C�Y(byte�P��), �z�������͏������܂ꂸ���������
//		bCharSize			�����T�C�Y�w��(MSZ:0x89, NSZ:0x8A)��t�^���邩�ۂ�
//							FALSE���w�肷��ƕ����T�C�Y�w����ȗ�����(���ׂđS�p�ŕ\��)
//		�߂�l				�ϊ����Đ�������������̒���(BYTE�P��)
//							dbuf��NULL���w�肷��ƕϊ����Đ�������������̒���(BYTE�P��)�����Ԃ�

//		conv_from_unicode1��conv_from_unicode2���Ă�ŁC�ϊ����ʂ̒Z�������̗p����
//		�ϊ����ʂ̒����ɍS��Ȃ��Ȃ璼�ڂǂ��炩�𒼐ڌĂ�ł���
//		conv_from_unicode3�͑��̓���Z���ϊ����ʂ𓾂���ꍇ�����邪�����e�L�X�g�ł͕ϊ����Ԃ����ɒ����Ȃ��Ă��܂��\������

/*
	int		len1 = conv_from_unicode1(NULL, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
	int		len2 = conv_from_unicode2(NULL, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
#ifdef USE_CONV_FROM_UNICODE3
	int		len3 = conv_from_unicode3(NULL, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
#endif

	int		len;

#ifndef USE_CONV_FROM_UNICODE3
	if(len1 <= len2) {
		len = conv_from_unicode1(dbuf, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
	} else {
		len = conv_from_unicode2(dbuf, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
	}
#else
	if( (len1 <= len2) && (len1 <= len3) ) {
		len = conv_from_unicode1(dbuf, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
	} else if( (len2 <= len1) && (len2 <= len3) ) {
		len = conv_from_unicode2(dbuf, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
	} else {
		len = conv_from_unicode3(dbuf, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);
	}
#endif
*/
	int		len = conv_from_unicode2(dbuf, maxbufsize, sbuf, total_length, bCharSize, NORMAL_NEWLINE_CODE);

	return len;
}


int conv_from_unicode1(unsigned char *dbuf, const int maxbufsize, const WCHAR *sbuf, const int total_length, const BOOL bCharSize, const BOOL bNLChar)
{

//		UNICODE(UTF-16LE)������ -> 8�P�ʕ���������ւ̕ϊ��@����1
//
//		sbuf				�ϊ���buf
//		total_length		���̒���(WCHAR�P��, NULL���������܂܂Ȃ�)
//		dbuf				�ϊ���buf
//		maxbufsize			�ϊ���buf�̍ő�T�C�Y(byte�P��), �z�������͏������܂ꂸ���������
//		bCharSize			�����T�C�Y�w��(MSZ:0x89, NSZ:0x8A)��t�^���邩�ۂ�
//							FALSE���w�肷��ƕ����T�C�Y�w����ȗ�����(���ׂđS�p�ŕ\��)
//		bNLChar				TRUE�Ȃ���s�R�[�h��0x0D���g�p, FALSE�Ȃ�0x0A���g�p����
//		�߂�l				�ϊ����Đ�������������̒���(BYTE�P��)
//							dbuf��NULL���w�肷��ƕϊ����Đ�������������̒���(BYTE�P��)�����Ԃ�
//
//		REGION_GL			BANK_G0, BANK_G1�Ő؂�ւ�
//		REGION_GR			BANK_G2, BANK_G3�Ő؂�ւ�
//		BANK_G0				F_KANJI, F_JIS1KANJI, F_JIS2KANJI, F_TUIKAKIGOU�Ő؂�ւ�
//		BANK_G1				F_ALPHA, F_HANKAKU�Ő؂�ւ�
//		BANK_G2				F_HIRAGANA�Œ�
//		BANK_G3				F_KATAKANA�Œ�

	ConvStatus	sta;
	initConvStatus(&sta);

	int		src = 0;
	int		dst = 0;


	// �ϊ����C��

	while(src < total_length)
	{
		int		jiscode;
		int		src_char_len;

		int		fcode = charclass(sbuf + src, &jiscode, &src_char_len);

		switch(fcode)
		{
			case F_CONTROL:
				switch(jiscode)
				{
					case 0x09:	// TAB
						CHARBUF(0x09);
						break;
					case 0x0A:	// LF
						if(bNLChar) {
							CHARBUF(0x0D);
						} else {
							CHARBUF(0x0A);
						}
						break;
					case 0x20:	// SP
						IF_NSZ_TO_MSZ;
						CHARBUF(0x20);
						break;
					default:
						break;
				}
				break;

			case F_ALPHA:
				if(sta.region[REGION_GL] != BANK_G1) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_NSZ_TO_MSZ;
				if(sta.bank[BANK_G1] != F_ALPHA) {
					CHARBUF(0x1B);
					CHARBUF(0x29);
					CHARBUF(F_ALPHA);						// F_ALPHA -> G1
					sta.bank[BANK_G1] = F_ALPHA;
				}
				CHARBUF(jiscode);
				break;

			case F_KANJI:
				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL	(LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G0] != F_KANJI) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_KANJI);						// F_KANJI -> G0
					sta.bank[BANK_G0] = F_KANJI;
				}
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_JIS1KANJI:
				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G0] != F_JIS1KANJI) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_JIS1KANJI);					// F_JIS1KANJI -> G0
					sta.bank[BANK_G0] = F_JIS1KANJI;
				}
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_JIS2KANJI:
				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G0] != F_JIS2KANJI) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_JIS2KANJI);					// F_JIS2KANJI -> G0
					sta.bank[BANK_G0] = F_JIS2KANJI;
				}
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_HANKAKU:
				if(sta.region[REGION_GL] != BANK_G1) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_NSZ_TO_MSZ;
				if(sta.bank[BANK_G1] != F_HANKAKU) {
					CHARBUF(0x1B);
					CHARBUF(0x29);
					CHARBUF(F_HANKAKU);						// F_HANKAKU -> G1
					sta.bank[BANK_G1] = F_HANKAKU;
				}
				CHARBUF(jiscode);
				break;

			case F_TUIKAKIGOU:
				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G0] != F_TUIKAKIGOU) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_TUIKAKIGOU);					// F_TUIKAKIGOU -> G0
					sta.bank[BANK_G0] = F_TUIKAKIGOU;
				}
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_KANACOMMON:
				IF_MSZ_TO_NSZ;
				CHARBUF(jiscode + 0x80);					// REGION_GR��BANK_G2(F_HIRAGANA),BANK_G3(F_KATAKANA)�̂ǂ���ł��ǂ�				
				break;

			case F_HIRAGANA:

				if(sta.region[REGION_GR] != BANK_G2) {

					// REGION_GR��BANK_G2(F_HIRAGANA)�ɐ؂�ւ���ׂ������f�A�K�v�Ȃ�؂�ւ�

					if(kanaChange(sbuf, src, total_length, F_HIRAGANA)) {
						CHARBUF(0x1B);
						CHARBUF(0x7D);							// G2 -> GR (LS2R)
						sta.region[REGION_GR] = BANK_G2;
					}
				}

				// REGION_GR��BANK_G2�Ȃ炻�̂܂܏o�͂���

				if(sta.region[REGION_GR] == BANK_G2) {
					IF_MSZ_TO_NSZ;
					CHARBUF(jiscode + 0x80);
					break;
				}

				// ����ȊO�̏ꍇ��BANK_G0���g����F_KANJI�ŏo��

				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G0] != F_KANJI) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_KANJI);						// F_KANJI -> G0
					sta.bank[BANK_G0] = F_KANJI;
				}
				jiscode = jis12conv(hiragana1conv(jiscode, TRUE), FALSE);
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_KATAKANA:

				if(sta.region[REGION_GR] != BANK_G3) {

					// REGION_GR��BANK_G3(F_KATAKANA)�ɐ؂�ւ���ׂ������f�A�K�v�Ȃ�؂�ւ�

					if(kanaChange(sbuf, src, total_length, F_KATAKANA)) {
						CHARBUF(0x1B);
						CHARBUF(0x7C);							// G3 -> GR (LS3R)
						sta.region[REGION_GR] = BANK_G3;
					}
				}

				// REGION_GR��BANK_G3�Ȃ炻�̂܂܏o�͂���

				if(sta.region[REGION_GR] == BANK_G3) {
					IF_MSZ_TO_NSZ;
					CHARBUF(jiscode + 0x80);
					break;
				}

				// ����ȊO�̏ꍇ��BANK_G0���g����F_KANJI�ŏo��

				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G0] != F_KANJI) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_KANJI);						// F_KANJI -> G0
					sta.bank[BANK_G0] = F_KANJI;
				}
				jiscode = jis12conv(katakana1conv(jiscode, TRUE), FALSE);
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			default:
				break;
		}

		src += src_char_len;

	}

	CHARBUF(0x00);

	dst--;
	if(dst > maxbufsize) dst = maxbufsize; 

	return dst;			// �ϊ���̒�����Ԃ�(byte�P��), �I�[�̃k�����������܂܂Ȃ�
}


int conv_from_unicode2(unsigned char *dbuf, const int maxbufsize, const WCHAR *sbuf, const int total_length, const BOOL bCharSize, const BOOL bNLChar)
{

//		UNICODE(UTF-16LE)������ -> 8�P�ʕ���������ւ̕ϊ��@����2
//
//		sbuf				�ϊ���buf
//		total_length		���̒���(WCHAR�P��, NULL���������܂܂Ȃ�)
//		dbuf				�ϊ���buf
//		maxbufsize			�ϊ���buf�̍ő�T�C�Y(byte�P��), �z�������͏������܂ꂸ���������
//		bCharSize			�����T�C�Y�w��(MSZ:0x89, NSZ:0x8A)��t�^���邩�ۂ�
//							FALSE���w�肷��ƕ����T�C�Y�w����ȗ�����(���ׂđS�p�ŕ\��)
//		bNLChar				TRUE�Ȃ���s�R�[�h��0x0D���g�p, FALSE�Ȃ�0x0A���g�p����
//		�߂�l				�ϊ����Đ�������������̒���(BYTE�P��)
//							dbuf��NULL���w�肷��ƕϊ����Đ�������������̒���(BYTE�P��)�����Ԃ�
//
//		REGION_GL			BANK_G0, BANK_G1�Ő؂�ւ�
//		REGION_GR			BANK_G2, BANK_G3�Ő؂�ւ�
//		BANK_G0				F_KANJI(�ق�)�Œ�
//		BANK_G1				F_ALPHA, F_JIS1KANJI, F_JIS2KANJI, F_TUIKAKIGOU, F_HANKAKU�Ő؂�ւ�
//		BANK_G2				F_HIRAGANA�Œ�
//		BANK_G3				F_KATAKANA�Œ�

	ConvStatus	sta;
	initConvStatus(&sta);

	int		src = 0;
	int		dst = 0;


	// �ϊ����C��

	while(src < total_length)
	{
		int		jiscode;
		int		src_char_len;

		int		fcode = charclass(sbuf + src, &jiscode, &src_char_len);

		switch(fcode)
		{
			case F_CONTROL:
				switch(jiscode)
				{
					case 0x09:	// TAB
						CHARBUF(0x09);
						break;
					case 0x0A:	// LF
						if(bNLChar) {
							CHARBUF(0x0D);
						} else {
							CHARBUF(0x0A);
						}
						break;
					case 0x20:	// SP
						IF_NSZ_TO_MSZ;
						CHARBUF(0x20);
						break;
					default:
						break;
				}
				break;

			case F_ALPHA:
				if(sta.region[REGION_GL] != BANK_G1) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_NSZ_TO_MSZ;
				if(sta.bank[BANK_G1] != F_ALPHA) {
					CHARBUF(0x1B);
					CHARBUF(0x29);
					CHARBUF(F_ALPHA);						// F_ALPHA -> G1
					sta.bank[BANK_G1] = F_ALPHA;
				}
				CHARBUF(jiscode);
				break;

			case F_KANJI:
				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL	(LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_JIS1KANJI:
				if(sta.region[REGION_GL] != BANK_G1) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G1] != F_JIS1KANJI) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(0x29);
					CHARBUF(F_JIS1KANJI);					// F_JIS1KANJI -> G1
					sta.bank[BANK_G1] = F_JIS1KANJI;
				}
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_JIS2KANJI:
				if(sta.region[REGION_GL] != BANK_G1) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G1] != F_JIS2KANJI) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(0x29);
					CHARBUF(F_JIS2KANJI);					// F_JIS2KANJI -> G1
					sta.bank[BANK_G1] = F_JIS2KANJI;
				}
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_HANKAKU:
				if(sta.region[REGION_GL] != BANK_G1) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_NSZ_TO_MSZ;
				if(sta.bank[BANK_G1] != F_HANKAKU) {
					CHARBUF(0x1B);
					CHARBUF(0x29);
					CHARBUF(F_HANKAKU);						// F_HANKAKU -> G1
					sta.bank[BANK_G1] = F_HANKAKU;
				}
				CHARBUF(jiscode);
				break;

			case F_TUIKAKIGOU:
/*
				if( (sta.bank[BANK_G0] != F_TUIKAKIGOU) && (sta.bank[BANK_G1] != F_TUIKAKIGOU) ) {

					// BANK_G0��F_TUIKAKIGOU�ɐ؂�ւ���ׂ������f
					// �ȍ~��F_TUIKAKIGOU, F_ALPHA, F_CONTROL�ȊO�̕�����������ΐ؂�ւ�

					if(kigouChange(sbuf, src, total_length)) {
						CHARBUF(0x1B);
						CHARBUF(0x24);
						CHARBUF(F_TUIKAKIGOU);				// F_TUIKAKIGOU -> G0
						sta.bank[BANK_G0] = F_TUIKAKIGOU;
					}
				}

				// BANK_G0��F_TUIKAKIGOU�ɂȂ��Ă����BANK_G0�ŏo��

				if(sta.bank[BANK_G0] == F_TUIKAKIGOU) {
					if(sta.region[REGION_GL] != BANK_G0) {
						CHARBUF(0x0F);							// G0 -> GL (LS0)
						sta.region[REGION_GL] = BANK_G0;
					}
					IF_MSZ_TO_NSZ;
					CHARBUF(jiscode >> 8);
					CHARBUF(jiscode & 0xFF);
					break;
				}
*/				
				// �����łȂ��ꍇ�͒ʏ��BANK_G1�ŏo��

				if(sta.region[REGION_GL] != BANK_G1) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_MSZ_TO_NSZ;
				if(sta.bank[BANK_G1] != F_TUIKAKIGOU) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(0x29);
					CHARBUF(F_TUIKAKIGOU);					// F_TUIKAKIGOU -> G1
					sta.bank[BANK_G1] = F_TUIKAKIGOU;
				}
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_KANACOMMON:
				IF_MSZ_TO_NSZ;
				CHARBUF(jiscode + 0x80);					// REGION_GR��BANK_G2(F_HIRAGANA),BANK_G3(F_KATAKANA)�̂ǂ���ł��ǂ�				
				break;

			case F_HIRAGANA:

				if(sta.region[REGION_GR] != BANK_G2) {

					// REGION_GR��BANK_G2(F_HIRAGANA)�ɐ؂�ւ���ׂ������f�A�K�v�Ȃ�؂�ւ�

					if(kanaChange(sbuf, src, total_length, F_HIRAGANA)) {
						CHARBUF(0x1B);
						CHARBUF(0x7D);							// G2 -> GR (LS2R)
						sta.region[REGION_GR] = BANK_G2;
					}
				}

				// REGION_GR��BANK_G2�Ȃ炻�̂܂܏o�͂���

				if(sta.region[REGION_GR] == BANK_G2) {
					IF_MSZ_TO_NSZ;
					CHARBUF(jiscode + 0x80);
					break;
				}

				// ����ȊO�̏ꍇ��BANK_G0���g����F_KANJI�ŏo��

				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				jiscode = jis12conv(hiragana1conv(jiscode, TRUE), FALSE);
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_KATAKANA:

				if(sta.region[REGION_GR] != BANK_G3) {

					// REGION_GR��BANK_G3(F_KATAKANA)�ɐ؂�ւ���ׂ������f�A�K�v�Ȃ�؂�ւ�

					if(kanaChange(sbuf, src, total_length, F_KATAKANA)) {
						CHARBUF(0x1B);
						CHARBUF(0x7C);							// G3 -> GR (LS3R)
						sta.region[REGION_GR] = BANK_G3;
					}
				}

				// REGION_GR��BANK_G3�Ȃ炻�̂܂܏o�͂���

				if(sta.region[REGION_GR] == BANK_G3) {
					IF_MSZ_TO_NSZ;
					CHARBUF(jiscode + 0x80);
					break;
				}

				// ����ȊO�̏ꍇ��BANK_G0���g����F_KANJI�ŏo��

				if(sta.region[REGION_GL] != BANK_G0) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				IF_MSZ_TO_NSZ;
				jiscode = jis12conv(katakana1conv(jiscode, TRUE), FALSE);
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			default:
				break;
		}

		src += src_char_len;

	}

	CHARBUF(0x00);

	dst--;
	if(dst > maxbufsize) dst = maxbufsize; 

	return dst;			// �ϊ���̒�����Ԃ�(byte�P��), �I�[�̃k�����������܂܂Ȃ�
}


int conv_from_unicode3(unsigned char *dbuf, const int maxbufsize, const WCHAR *sbuf, const int total_length, const BOOL bCharSize, const BOOL bNLChar)
{

//		UNICODE(UTF-16LE)������ -> 8�P�ʕ���������ւ̕ϊ��@����3
//
//		sbuf				�ϊ���buf
//		total_length		���̒���(WCHAR�P��, NULL���������܂܂Ȃ�)
//		dbuf				�ϊ���buf
//		maxbufsize			�ϊ���buf�̍ő�T�C�Y(byte�P��), �z�������͏������܂ꂸ���������
//		bCharSize			�����T�C�Y�w��(MSZ:0x89, NSZ:0x8A)��t�^���邩�ۂ�
//							FALSE���w�肷��ƕ����T�C�Y�w����ȗ�����(���ׂđS�p�ŕ\��)
//		bNLChar				TRUE�Ȃ���s�R�[�h��0x0D���g�p, FALSE�Ȃ�0x0A���g�p����
//		�߂�l				�ϊ����Đ�������������̒���(BYTE�P��)
//							dbuf��NULL���w�肷��ƕϊ����Đ�������������̒���(BYTE�P��)�����Ԃ�
//
//		REGION_GL			BANK_G0, BANK_G1�Ő؂�ւ�
//		REGION_GR			BANK_G2, BANK_G3�Ő؂�ւ�
//		BANK_G0, BANK_G1 	F_KANJI, F_ALPHA, F_JIS1KANJI, F_JIS2KANJI, F_TUIKAKIGOU, F_HANKAKU�Ő؂�ւ�
//		BANK_G2				F_HIRAGANA�Œ�
//		BANK_G3				F_KATAKANA�Œ�

	ConvStatus	sta;
	initConvStatus(&sta);

	int		src = 0;
	int		dst = 0;


	// �ϊ����C��

	while(src < total_length)
	{
		int		jiscode;
		int		src_char_len;

		int		fcode = charclass(sbuf + src, &jiscode, &src_char_len);

		switch(fcode)
		{
			case F_CONTROL:
				switch(jiscode)
				{
					case 0x09:	// TAB
						CHARBUF(0x09);
						break;
					case 0x0A:	// LF
						if(bNLChar) {
							CHARBUF(0x0D);
						} else {
							CHARBUF(0x0A);
						}
						break;
					case 0x20:	// SP
						IF_NSZ_TO_MSZ;
						CHARBUF(0x20);
						break;
					default:
						break;
				}
				break;

			case F_ALPHA:
			case F_HANKAKU:

				// BANK_G0, BANK_G1�̂ǂ�����K�v��fcode�łȂ��ꍇ�A�ǂ����؂�ւ��邩���f����

				if( (sta.bank[BANK_G0] != fcode) && (sta.bank[BANK_G1] != fcode) ) {
					if(bankChange(sbuf, src, total_length, &sta, fcode)) {
						CHARBUF(0x1B);
						CHARBUF(0x28);
						CHARBUF(fcode);					// 1 Byte G Set -> G0
						sta.bank[BANK_G0] = fcode;
					} else {
						CHARBUF(0x1B);
						CHARBUF(0x29);
						CHARBUF(fcode);					// 1 Byte G Set -> G1
						sta.bank[BANK_G1] = fcode;
					}
				}

				// �K�v�Ȃ�LS0, LS1

				if( (sta.bank[BANK_G0] == fcode) && (sta.region[REGION_GL] != BANK_G0) ) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				if( (sta.bank[BANK_G1] == fcode) && (sta.region[REGION_GL] != BANK_G1) ) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_NSZ_TO_MSZ;
				CHARBUF(jiscode);
				break;

			case F_KANJI:
			case F_JIS1KANJI:
			case F_JIS2KANJI:
			case F_TUIKAKIGOU:

				// BANK_G0, BANK_G1�̂ǂ�����K�v��fcode�łȂ��ꍇ�A�ǂ����؂�ւ��邩���f����

				if( (sta.bank[BANK_G0] != fcode) && (sta.bank[BANK_G1] != fcode) ) {
					if(bankChange(sbuf, src, total_length, &sta, fcode)) {
						CHARBUF(0x1B);
						CHARBUF(0x24);
						CHARBUF(fcode);					// 2 Byte G Set -> G0
						sta.bank[BANK_G0] = fcode;
					} else {
						CHARBUF(0x1B);
						CHARBUF(0x24);
						CHARBUF(0x29);
						CHARBUF(fcode);					// 2 Byte G Set -> G1
						sta.bank[BANK_G1] = fcode;
					}
				}

				// �K�v�Ȃ�LS0, LS1

				if( (sta.bank[BANK_G0] == fcode) && (sta.region[REGION_GL] != BANK_G0) ) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				if( (sta.bank[BANK_G1] == fcode) && (sta.region[REGION_GL] != BANK_G1) ) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_MSZ_TO_NSZ;
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_KANACOMMON:
				IF_MSZ_TO_NSZ;
				CHARBUF(jiscode + 0x80);					// REGION_GR��BANK_G2(F_HIRAGANA),BANK_G3(F_KATAKANA)�̂ǂ���ł��ǂ�				
				break;

			case F_HIRAGANA:

				if(sta.region[REGION_GR] != BANK_G2) {

					// REGION_GR��BANK_G2(F_HIRAGANA)�ɐ؂�ւ���ׂ������f�A�K�v�Ȃ�؂�ւ�

					if(kanaChange(sbuf, src, total_length, F_HIRAGANA)) {
						CHARBUF(0x1B);
						CHARBUF(0x7D);							// G2 -> GR (LS2R)
						sta.region[REGION_GR] = BANK_G2;
					}
				}

				// REGION_GR��BANK_G2�Ȃ炻�̂܂܏o�͂���

				if(sta.region[REGION_GR] == BANK_G2) {
					IF_MSZ_TO_NSZ;
					CHARBUF(jiscode + 0x80);
					break;
				}

				// ����ȊO�̏ꍇ��BANK_G0��BANK_G1���g����F_KANJI�ŏo��

				if( (sta.bank[BANK_G0] != F_KANJI) && (sta.bank[BANK_G1] != F_KANJI) ) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_KANJI);						// F_KANJI -> G0
					sta.bank[BANK_G0] = F_KANJI;
				}
				if( (sta.bank[BANK_G0] == F_KANJI) && (sta.region[REGION_GL] != BANK_G0) ) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				if( (sta.bank[BANK_G1] == F_KANJI) && (sta.region[REGION_GL] != BANK_G1) ) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_MSZ_TO_NSZ;
				jiscode = jis12conv(hiragana1conv(jiscode, TRUE), FALSE);
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			case F_KATAKANA:

				if(sta.region[REGION_GR] != BANK_G3) {

					// REGION_GR��BANK_G3(F_KATAKANA)�ɐ؂�ւ���ׂ������f�A�K�v�Ȃ�؂�ւ�

					if(kanaChange(sbuf, src, total_length, F_KATAKANA)) {
						CHARBUF(0x1B);
						CHARBUF(0x7C);							// G3 -> GR (LS3R)
						sta.region[REGION_GR] = BANK_G3;
					}
				}

				// REGION_GR��BANK_G3�Ȃ炻�̂܂܏o�͂���

				if(sta.region[REGION_GR] == BANK_G3) {
					IF_MSZ_TO_NSZ;
					CHARBUF(jiscode + 0x80);
					break;
				}

				// ����ȊO�̏ꍇ��BANK_G0��BANK_G1���g����F_KANJI�ŏo��

				if( (sta.bank[BANK_G0] != F_KANJI) && (sta.bank[BANK_G1] != F_KANJI) ) {
					CHARBUF(0x1B);
					CHARBUF(0x24);
					CHARBUF(F_KANJI);						// F_KANJI -> G0
					sta.bank[BANK_G0] = F_KANJI;
				}
				if( (sta.bank[BANK_G0] == F_KANJI) && (sta.region[REGION_GL] != BANK_G0) ) {
					CHARBUF(0x0F);							// G0 -> GL (LS0)
					sta.region[REGION_GL] = BANK_G0;
				}
				if( (sta.bank[BANK_G1] == F_KANJI) && (sta.region[REGION_GL] != BANK_G1) ) {
					CHARBUF(0x0E);							// G1 -> GL (LS1)
					sta.region[REGION_GL] = BANK_G1;
				}
				IF_MSZ_TO_NSZ;
				jiscode = jis12conv(katakana1conv(jiscode, TRUE), FALSE);
				CHARBUF(jiscode >> 8);
				CHARBUF(jiscode & 0xFF);
				break;

			default:
				break;
		}

		src += src_char_len;

	}

	CHARBUF(0x00);

	dst--;
	if(dst > maxbufsize) dst = maxbufsize; 

	return dst;			// �ϊ���̒�����Ԃ�(byte�P��), �I�[�̃k�����������܂܂Ȃ�
}


int jis12conv(const int jiscode, const BOOL bConvDir)
{
//	JIS����񐅏������C�񊿎��ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode (�� 0x2121 -> U+3000)
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int jis12table[] = 
	{
		#include "jis12table.h"
	};

	static int		jis12revtable[sizeof(jis12table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(jis12table, jis12revtable, sizeof(jis12table));
		bTableInitialized = TRUE;
	}

	int		winresult = jis12winconv(jiscode, bConvDir);
	if(winresult != 0) return winresult;
	
	void	*result;
	if(bConvDir) {
		result = bsearch(&jiscode, jis12table, sizeof(jis12table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&jiscode, jis12revtable, sizeof(jis12revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


int jis12winconv(const int jiscode, const BOOL bConvDir)
{
//	JIS����񐅏��񊿎���windows�ŗL�̃}�b�s���O��L������̂ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int jis12wintable[] = 
	{
		#include "jis12wintable.h"
	};

	static int		jis12winrevtable[sizeof(jis12wintable) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(jis12wintable, jis12winrevtable, sizeof(jis12wintable));
		bTableInitialized = TRUE;
	}
	
	void	*result;
	if(bConvDir) {
		result = bsearch(&jiscode, jis12wintable, sizeof(jis12wintable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&jiscode, jis12winrevtable, sizeof(jis12winrevtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


int jis3conv(const int jiscode, const BOOL bConvDir)
{
//	JIS��O���������񊿎����Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int jis3table[] = 
	{
		#include "jis3table.h"
	};

	static int		jis3revtable[sizeof(jis3table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(jis3table, jis3revtable, sizeof(jis3table));
		bTableInitialized = TRUE;
	}
	
	void	*result;
	if(bConvDir) {
		result = bsearch(&jiscode, jis3table, sizeof(jis3table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&jiscode, jis3revtable, sizeof(jis3revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


int jis3combconv(const int jiscode, WCHAR* dbuf, const int bufsize)
{
//	JIS��O���������񊿎���unicode���������ɑΉ�������̂ɂ��Ă� jiscode -> UTF-16������ϊ�
//	�Ή����镶���R�[�h���Ȃ���Ζ߂�l0��Ԃ�
//
//	jis         �ϊ���jiscode
//	dbuf		�ϊ����������񂪏������܂��buf
//  bufsize     �ϊ���dbuf�̍ő�T�C�Y(WCHAR�P��)
//	�߂�l		�ϊ�����UTF-16������(WCHAR�P��)

	static WCHAR jis3combtable[][UTF16TABLELEN] =
	{
		#include "jis3combiningtable.h"
	};
    
	int		len = 0;
	WCHAR	jisbuf[UTF16TABLELEN];
 
	jisbuf[0] = (WCHAR)jiscode;

	void	*result = bsearch(jisbuf, jis3combtable, sizeof(jis3combtable) / sizeof(WCHAR) / UTF16TABLELEN / 2, sizeof(WCHAR) * UTF16TABLELEN * 2, comparefortable2);

	if(result != NULL) len = swprintf_s(dbuf, bufsize, L"%s", (WCHAR*)result + UTF16TABLELEN);

	return len;
}


int jis3combrevconv(const WCHAR* sbuf, int *jis)
{
//	JIS��O���������񊿎���unicode���������ɑΉ�������̂ɂ��Ă� UTF-16 -> jiscode�ϊ�
//	�Ή����镶���R�[�h���Ȃ���Ζ߂�l0��Ԃ�
//
//	sbuf		�ϊ���buf
//  jis         �ϊ����ꂽjis�R�[�h������   
//	�߂�l		�ϊ�����UTF-16������(WCHAR�P��)

    static WCHAR jis3combtable[][UTF16TABLELEN] =
	{
        #include "jis3combiningtable.h"
	};    
    
	static BOOL		bTableInitialized = FALSE;
    
	if(!bTableInitialized)
    {
        qsort(jis3combtable, sizeof(jis3combtable) / sizeof(WCHAR) / UTF16TABLELEN / 2, sizeof(WCHAR) * UTF16TABLELEN * 2, comparefortable2str);
		bTableInitialized = TRUE;
	}
    
    int     len;    
	void*   result = bsearch(sbuf - UTF16TABLELEN, jis3combtable, sizeof(jis3combtable) / sizeof(WCHAR) / UTF16TABLELEN / 2, sizeof(WCHAR) * UTF16TABLELEN * 2, comparefortable2str);
    
	if(result != NULL)
    {
		len = (int)wcsnlen_s((WCHAR*)result + UTF16TABLELEN, UTF16TABLELEN);

        if( !wcsncmp(sbuf, (WCHAR*)result + UTF16TABLELEN, len) ) {    // ������v�̔r��
			*jis = (int)(*(WCHAR*)result);
            return len;
        }
	}
 
    *jis = 0;
    
    return 0;
}


int jis4conv(const int jiscode, const BOOL bConvDir)
{
//	JIS��l���������ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int jis4table[] = 
	{
		#include "jis4table.h"
	};

	static int		jis4revtable[sizeof(jis4table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(jis4table, jis4revtable, sizeof(jis4table));
		bTableInitialized = TRUE;
	}
	
	void	*result;
	if(bConvDir) {
		result = bsearch(&jiscode, jis4table, sizeof(jis4table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&jiscode, jis4revtable, sizeof(jis4revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


int hiragana1conv(const int code, const BOOL bConvDir)
{
//	�S�p�Ђ炪�ȕ����ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int hiragana1table[] = 
	{
		#include "hiragana1table.h"
	};

	static int		hiragana1revtable[sizeof(hiragana1table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(hiragana1table, hiragana1revtable, sizeof(hiragana1table));
		bTableInitialized = TRUE;
	}

	void	*result;
	if(bConvDir) {
		result = bsearch(&code, hiragana1table, sizeof(hiragana1table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&code, hiragana1revtable, sizeof(hiragana1revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


int katakana1conv(const int code, const BOOL bConvDir)
{
//	�S�p�J�^�J�i�����ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int katakana1table[] = 
	{
		#include "katakana1table.h"
	};

	static int		katakana1revtable[sizeof(katakana1table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(katakana1table, katakana1revtable, sizeof(katakana1table));
		bTableInitialized = TRUE;
	}

	void	*result;
	if(bConvDir) {
		result = bsearch(&code, katakana1table, sizeof(katakana1table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&code, katakana1revtable, sizeof(katakana1revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}

int kanacommon1conv(const int code, const BOOL bConvDir)
{
//	�S�p�Ђ炪�ȁC�J�^�J�i�W���̋��ʕ����ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int kanacommon1table[] = 
	{
		#include "kanacommon1table.h"
	};

	static int		kanacommon1revtable[sizeof(kanacommon1table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(kanacommon1table, kanacommon1revtable, sizeof(kanacommon1table));
		bTableInitialized = TRUE;
	}

	void	*result;
	if(bConvDir) {
		result = bsearch(&code, kanacommon1table, sizeof(kanacommon1table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&code, kanacommon1revtable, sizeof(kanacommon1revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


int hankaku1conv(const int code, const BOOL bConvDir)
{
//	JIS X0201�J�^�J�i�����W���ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int hankaku1table[] = 
	{
		#include "hankaku1table.h"
	};

	static int		hankaku1revtable[sizeof(hankaku1table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(hankaku1table, hankaku1revtable, sizeof(hankaku1table));
		bTableInitialized = TRUE;
	}

	void	*result;
	if(bConvDir) {
		result = bsearch(&code, hankaku1table, sizeof(hankaku1table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&code, hankaku1revtable, sizeof(hankaku1revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


int tuikakigou1conv(const int code, const BOOL bConvDir)
{
//	�ǉ��L���C�ǉ������W���̂����A�Ή�����unicode���������݂��镶���ɂ��Ă� jiscode <-> unicode�ϊ�
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		����jiscode -> �߂�lunicode
//	bConvDir : FALSE	����unicode -> �߂�ljiscode

	static int kigou1table[] = 
	{
		#include "tuikakigou1table.h"
	};

#if	defined(__TOOL_VISTA_7__) || defined(__TOOL_XP__)
	static int kigou1excludetable[] =
	{
		#include "kigou1excludetable.h"
	};

	static BOOL		bExcludeTableInitialized = FALSE;
#endif

	static int		kigou1revtable[sizeof(kigou1table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(kigou1table, kigou1revtable, sizeof(kigou1table));
		bTableInitialized = TRUE;
	}

#if	defined(__TOOL_VISTA_7__) || defined(__TOOL_XP__)
	if(bConvDir && !bExcludeTableInitialized) {
		qsort(kigou1excludetable, sizeof(kigou1excludetable) / sizeof(int), sizeof(int), comparefortable);
		bExcludeTableInitialized = TRUE;
	}
#endif

	void	*result;
	if(bConvDir) {
		result = bsearch(&code, kigou1table, sizeof(kigou1table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&code, kigou1revtable, sizeof(kigou1revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	int		resultcode = *((int*)result + 1);

#if	defined(__TOOL_VISTA_7__) || defined(__TOOL_XP__)
	result = bsearch(&resultcode, kigou1excludetable, sizeof(kigou1excludetable) / sizeof(int), sizeof(int), comparefortable);
	if(result != NULL) resultcode = 0;
#endif

	return resultcode;
}


int tuikakigou2conv(const int jis, WCHAR *dbuf, const int bufsize)
{
//	�ǉ��L���W���̂����A�Ή�����unicode���������݂��镶�������݂���
//	[...]���邢��[#xx#xx]�ŕ\����镶���ɂ��Ă� jiscode -> UTF-16������ϊ�
//
//	jis         �ϊ���jiscode
//	dbuf		�ϊ����������񂪏������܂��buf
//  bufsize     �ϊ���dbuf�̃T�C�Y(WCHAR�P��)
//	�߂�l		�ϊ�����unicode������(WCHAR�P��)

	static WCHAR kigou2table[][UTF16TABLELEN] =
	{
        #include "tuikakigou2table.h"
	};
    
	int			len;
	WCHAR		jisbuf[UTF16TABLELEN];
    
    jisbuf[0] = (WCHAR)jis;
    
	void	*result = bsearch(jisbuf, kigou2table, sizeof(kigou2table) / sizeof(WCHAR) / UTF16TABLELEN / 2, sizeof(WCHAR) * UTF16TABLELEN * 2, comparefortable2);
    
	if(result != NULL) {
        len = swprintf_s(dbuf, bufsize, L"%s", (WCHAR*)result + UTF16TABLELEN);
	} else {
		len = swprintf_s(dbuf, bufsize, L"[#%.2d#%.2d]", (jis / 256) - 32, (jis % 256) - 32);
	}
    
	return len;
}


int tuikakigou2revconv(const WCHAR *sbuf, int *jis)
{
//	�ǉ��L���W���̂����A�Ή�����unicode���������݂��镶�������݂���
//	[...]���邢��[#xx#xx]�ŕ\����镶���ɂ��Ă� UTF-16������ -> jiscode�ϊ�
//
//	sbuf		�ϊ����̕������L����buf
//  jis         �ϊ����ꂽjis�R�[�h������   
//	�߂�l		�ϊ�����UTF-16������(WCHAR�P��)

    static WCHAR kigou2table[][UTF16TABLELEN] =
	{
        #include "tuikakigou2table.h"
	};    
    
	static BOOL		bTableInitialized = FALSE;
    
	if(!bTableInitialized)
    {
        qsort(kigou2table, sizeof(kigou2table) / sizeof(WCHAR) / UTF16TABLELEN / 2, sizeof(WCHAR) * UTF16TABLELEN * 2, comparefortable2str);
		bTableInitialized = TRUE;
	}
    
    int     len;    
	void*   result = bsearch(sbuf - UTF16TABLELEN, kigou2table, sizeof(kigou2table) / sizeof(WCHAR) / UTF16TABLELEN / 2, sizeof(WCHAR) * UTF16TABLELEN * 2, comparefortable2str);

    if(result != NULL)
    {
		len = (int)wcsnlen_s((WCHAR*)result + UTF16TABLELEN, UTF16TABLELEN);
        
        if( !wcsncmp(sbuf, (WCHAR*)result + UTF16TABLELEN, len) ) {							// ������v�̔r��
            *jis = *(WCHAR*)result;
            return len;
        }
	}
    
    if( (sbuf[0]==L'[') && (sbuf[1]==L'#') && isdigit(sbuf[2]) && isdigit(sbuf[3]) && (sbuf[4]==L'#') && isdigit(sbuf[5]) && isdigit(sbuf[6]) && (sbuf[7]==L']') )
    {
        *jis = ((sbuf[2] - L'0') * 10 + (sbuf[3] - L'0')) * 256 + (sbuf[5] - L'0') * 10 + (sbuf[6] - L'0') + 0x2020;
        return 8;
    } 

    *jis = 0;
    
    return 0;
}


int charsize1conv(const int jiscode, const BOOL bConvDir)
{
//�@�󔒕����y�щp�������� ���pjiscode <-> �S�pjiscode �ϊ�
//�@��FA(0x41) <-> �`(0x2341)
//	�Ή����镶���R�[�h���Ȃ����0��Ԃ�
//
//	bConvDir : TRUE		�������pjiscode -> �߂�l�S�pjiscode
//	bConvDir : FALSE	�����S�pjiscode -> �߂�l���pjiscode

	static int charsize1table[] = 
	{
		#include "charsize1table.h"
	};

	static int		charsize1revtable[sizeof(charsize1table) / sizeof(int)];
	static BOOL		bTableInitialized = FALSE;

	if(!bConvDir && !bTableInitialized) {
		initrevtable(charsize1table, charsize1revtable, sizeof(charsize1table));
		bTableInitialized = TRUE;
	}

	void	*result;
	if(bConvDir) {
		result = bsearch(&jiscode, charsize1table, sizeof(charsize1table) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	} else {
		result = bsearch(&jiscode, charsize1revtable, sizeof(charsize1revtable) / sizeof(int) / 2, sizeof(int) * 2, comparefortable);
	}

	if(result == NULL) return 0;

	return *((int*)result + 1);
}


void initrevtable(const int *tabletop, int *revtop, const int tablesize)	// �ϊ��e�[�u���������p
{
	int		itemnum = tablesize / sizeof(int) / 2;
		
	for(int i = 0; i < itemnum; i++) {
		revtop[i * 2]		= tabletop[i * 2 + 1];
		revtop[i * 2 + 1]	= tabletop[i * 2];
	}

	qsort(revtop, itemnum, sizeof(int) * 2, comparefortable);
	
	return;
}


int comparefortable(const void *item1, const void *item2)					// �ϊ��e�[�u���\�[�g�C�����p�֐�
{
	return *(int*)item1 - *(int*)item2;
}


int comparefortable2(const void *item1, const void *item2)					// �ϊ��e�[�u���\�[�g�C�����p�֐�
{
	return *(WCHAR*)item1 - *(WCHAR*)item2;
}


int comparefortable2str(const void *item1, const void *item2)				// �ϊ��e�[�u���\�[�g�C�����p�֐�
{ 
    WCHAR *str1 = ((WCHAR*)item1) + UTF16TABLELEN;
    WCHAR *str2 = ((WCHAR*)item2) + UTF16TABLELEN;
    
    int		i = 0;
    
	while( (str1[i] != 0) && (str2[i] != 0) ) {
		if(str1[i] != str2[i]) return (int)(str1[i]) - (int)(str2[i]);
		i++;
	}
    
	return 0;
}


void initConvStatus(ConvStatus *status)
{
	status->region[REGION_GL]	= BANK_G0;
	status->region[REGION_GR]	= BANK_G2;
	
	status->bank[BANK_G0]		= F_KANJI;
	status->bank[BANK_G1]		= F_ALPHA;
	status->bank[BANK_G2]		= F_HIRAGANA;
	status->bank[BANK_G3]		= F_KATAKANA;

	status->bSingleShift		= FALSE;
	status->region_GL_backup	= BANK_G0;		// backup for singleshift

	status->bXCS				= FALSE;
	status->bNormalSize			= TRUE;			// TRUE: NSZ, FALSE: MSZ

	return;
}


BOOL isControlChar(const unsigned char c)
{
	if( (c >= 0x00) && (c <= 0x20) ) return TRUE;

	if( (c >= 0x7f) && (c <= 0xa0) ) return TRUE;

	if( c == 0xff ) return TRUE;

	return FALSE;
}


BOOL isOneByteGSET(const unsigned char c)
{
	BOOL	bResult;

	switch(c)
	{
		case F_ALPHA:
		case F_HIRAGANA:
		case F_KATAKANA:
		case F_MOSAICA:
		case F_MOSAICB:
		case F_MOSAICC:
		case F_MOSAICD:
		case F_P_ALPHA:
		case F_P_HIRAGANA:
		case F_P_KATAKANA:
		case F_HANKAKU:
			bResult = TRUE;
			break;
		default:
			bResult = FALSE;
	}

	return bResult;
}


BOOL isTwoByteGSET(const unsigned char c)
{
	BOOL	bResult;

	switch(c)
	{
		case F_KANJI:
		case F_JIS1KANJI:
		case F_JIS2KANJI:
		case F_TUIKAKIGOU:
			bResult = TRUE;
			break;
		default:
			bResult = FALSE;
	}

	return bResult;
}


BOOL isOneByteDRCS(const unsigned char c)
{
	BOOL	bResult;

	switch(c)
	{
		case F_DRCS1:
		case F_DRCS2:
		case F_DRCS3:
		case F_DRCS4:
		case F_DRCS5:
		case F_DRCS6:
		case F_DRCS7:
		case F_DRCS8:
		case F_DRCS9:
		case F_DRCS10:
		case F_DRCS11:
		case F_DRCS12:
		case F_DRCS13:
		case F_DRCS14:
		case F_DRCS15:
		case F_MACRO:
			bResult = TRUE;
			break;
		default:
			bResult = FALSE;
	}

	return bResult;
}


BOOL isTwoByteDRCS(const unsigned char c)
{
	BOOL	bResult;

	switch(c)
	{
		case F_DRCS0:
			bResult = TRUE;
			break;
		default:
			bResult = FALSE;
	}

	return bResult;
}


int numgbank(const unsigned char c)
{
	int		banknum;

	switch(c)
	{
		case 0x28:
			banknum = BANK_G0;
			break;
		case 0x29:
			banknum = BANK_G1;
			break;
		case 0x2A:
			banknum = BANK_G2;
			break;
		case 0x2B:
			banknum = BANK_G3;
			break;
		default:
			banknum = BANK_G0;
	}

	return banknum;
}


int wcharbuf(WCHAR *dbuf, const int maxbufsize, const int dst, const int uc, const BOOL bXCS)
{
	if(bXCS) return 0;

	if(uc >= 0x10000)
	{
		if( (dbuf != NULL) && (dst + 0 < maxbufsize) ) dbuf[dst + 0] = (WCHAR)(0xD800 + ((uc - 0x10000) / 0x400));
		if( (dbuf != NULL) && (dst + 1 < maxbufsize) ) dbuf[dst + 1] = (WCHAR)(0xDC00 + ((uc - 0x10000) % 0x400));

		return 2;
	}
	
	if( (dbuf != NULL) && (dst < maxbufsize) ) dbuf[dst] = (WCHAR)uc;

	return 1;
}


void charbuf(unsigned char *dbuf, const int maxbufsize, const int dst, const int code)
{
	if( (dbuf != NULL) && (dst < maxbufsize) ) dbuf[dst] = code;

	return;
}


void changeConvStatus(const unsigned char *sbuf, int *srcpos, ConvStatus *sta)
{
// ����R�[�h�ɏ]���ĕ����̌Ăяo���A�w�����䂷��
// srcpos�͐���R�[�h�T�C�Y���i��

	int		src = *srcpos;

	switch(sbuf[src])
	{
		// �����T�C�Y�w��

		case 0x89:
			sta->bNormalSize = FALSE;					// �����T�C�Y���p(MSZ)�w��
			src++;
			break;
		case 0x8A:
			sta->bNormalSize = TRUE;					// �����T�C�Y�S�p(NSZ)�w��
			src++;
			break;

		// �����̌Ăяo��

		case 0x0F:										// LS0 (0F), G0->GL ���b�L���O�V�t�g
			sta->region[REGION_GL] = BANK_G0;
			src++;
			break;
		case 0x0E:										// LS1 (0E), G1->GL ���b�L���O�V�t�g
			sta->region[REGION_GL] = BANK_G1;
			src++;
			break;
		case 0x19:										// SS2 (19), G2->GL �V���O���V�t�g
			sta->region_GL_backup = sta->region[REGION_GL];
			sta->region[REGION_GL] = BANK_G2;
			sta->bSingleShift = TRUE;
			src++;
			break;
		case 0x1D:										// SS3 (1D), G3->GL �V���O���V�t�g
			sta->region_GL_backup = sta->region[REGION_GL];
			sta->region[REGION_GL] = BANK_G3;
			sta->bSingleShift = TRUE;
			src++;
			break;

		case 0x1B:		// ESC�ɑ�������R�[�h

			switch(sbuf[src + 1])
			{
				// ESC�ɑ��������̌Ăяo��
				
				case 0x6E:								// LS2 (ESC 6E), G2->GL ���b�L���O�V�t�g
					sta->region[REGION_GL] = BANK_G2;
					src += 2;
					break;
				case 0x6F:								// LS3 (ESC 6F), G3->GL ���b�L���O�V�t�g
					sta->region[REGION_GL] = BANK_G3;
					src += 2;
					break;
				case 0x7E:								// LS1R (ESC 7E), G1->GR ���b�L���O�V�t�g
					sta->region[REGION_GR] = BANK_G1;
					src += 2;
					break;
				case 0x7D:								// LS2R (ESC 7D), G2->GR ���b�L���O�V�t�g
					sta->region[REGION_GR] = BANK_G2;
					src += 2;
					break;
				case 0x7C:								// LS3R (ESC 7C), G3->GR ���b�L���O�V�t�g
					sta->region[REGION_GR] = BANK_G3;
					src += 2;
					break;

				// ESC�ɑ��������̎w������

				case 0x28:	// ESC 28
				case 0x29:	// ESC 29
				case 0x2A:	// ESC 2A
				case 0x2B:	// ESC 2B
					if(isOneByteGSET(sbuf[src + 2])) {
						sta->bank[numgbank(sbuf[src + 1])] = sbuf[src + 2];					// 1�o�C�gGSET�w�� (ESC 28|29|2A|2B [F]) -> G0,G1,G2,G3
						src += 3;
					} else {
						if(sbuf[src + 2] == 0x20) {
							if(isOneByteDRCS(sbuf[src + 3])) {
								sta->bank[numgbank(sbuf[src + 1])] = sbuf[src + 3] + 0x10;	// 1�o�C�gDRCS�w�� (ESC 28|29|2A|2B 20 [F]) -> G0,G1,G2,G3		
								src += 4;														// + 0x10�͏I�[���������Ȃ��悤�ɂ��邽�߂̍׍H
							} else {
								src += 4;														// �s����1�o�C�gDRCS�w�� (ESC 28|29|2A|2B 20 XX)
							}
						} else {
							src += 3;															// �s����1�o�C�gGSET�w�� (ESC 28|29|2A|2B XX)
						}
					}
					break;

				case 0x24:	// ESC 24
					if(isTwoByteGSET(sbuf[src + 2])) {
						sta->bank[BANK_G0] = sbuf[src + 2];									// 2�o�C�gGSET�w�� (ESC 24 [F]) ->G0
						src += 3;
					} else {
						switch(sbuf[src + 2])
						{
							case 0x28:	// ESC 24 28
								if(sbuf[src + 3] == 0x20) {
									if(isTwoByteDRCS(sbuf[src + 4])) {
										sta->bank[BANK_G0] = sbuf[src + 4];					// 2�o�C�gDRCS�w�� (ESC 24 28 20 [F]) ->G0
										src += 5;
									} else {
										src += 5;												// �s����2�o�C�gDRCS�w�� (ESC 24 28 20 XX)
									}
								} else {
									src += 4;													// �s���Ȏw�� (ESC 24 28 XX)
								}
								break;
							case 0x29:	// ESC 24 29
							case 0x2A:	// ESC 24 2A
							case 0x2B:	// ESC 24 2B
								if(isTwoByteGSET(sbuf[src + 3])) {
									sta->bank[numgbank(sbuf[src + 2])] = sbuf[src + 3];		// 2�o�C�gGSET�w�� (ESC 24 29|2A|2B [F]) ->G1,G2,G3
									src += 4;
								} else {
									if(sbuf[src + 3] == 0x20) {
										if(isTwoByteDRCS(sbuf[src + 4])) {
											sta->bank[numgbank(sbuf[src + 2])] = sbuf[src + 4];	// 2�o�C�gDRCS�w�� (ESC 24 29|2A|2B 20 [F]) ->G1,G2,G3
											src += 5;
										} else {
											src += 5;												// �s����2�o�C�gDRCS�w�� (ESC 24 29|2A|2B 20 XX)
										}
									} else {
										src += 4;													// �s����2�o�C�gGSET�w�� (ESC 24 29|2A|2B XX)
									}
								}
								break;
							default:
								src += 3;															// �s���Ȏw�� (ESC 24 XX)
						}
					}
					break;

				default:
					src += 2;																		// �s���Ȏw�� (ESC XX)
			}
			break;

		default:	// ��L�ȊO�̐���R�[�h
			src++;
	}

	*srcpos = src;

	return;
}


void csiProc(const unsigned char *sbuf, int *srcpos, ConvStatus *sta)
{
// CSI����R�[�h����
// srcpos�͐���R�[�h�T�C�Y���i��
// 
// XCS�̏����̂�

	int			src = *srcpos + 1;

	int			param[4];
	memset(param, 0, sizeof(param));	

	int			pcount = 0;
	int			fcode = 0;
	
	while(TRUE) {

		if(isdigit(sbuf[src]) != 0) {										// �p�����[�^?
			param[pcount] *= 10;
			param[pcount] += (sbuf[src] - '0');
			src++;
		}
		else if(sbuf[src] == 0x3B) {										// �p�����[�^��؂�?
			pcount++;
			src++;
			if(pcount == 4) break;											// �p�����[�^������
		}
		else if(sbuf[src] == 0x20) {										// �p�����[�^�I��?
			if( (sbuf[src + 1] >= 0x42) && (sbuf[src + 1] <= 0x6F) ) fcode = sbuf[src + 1];		// �I�[����?
			src += 2;
			pcount++;
			break;
		}
		else if( (sbuf[src] >= 0x42) && (sbuf[src] <= 0x6F) ) {				// �I�[����?
			fcode = sbuf[src];
			src++;
			break;
		}
		else {																// �s���ȃR�[�h
			src++;
			break;
		}
	}

	switch(fcode) {

		case	0x66:									// XCS
			if( (pcount == 1) && (param[0] == 0) ) {
				sta->bXCS = TRUE;						// �O����֕�����`�J�n
				break;
			}
			if( (pcount == 1) && (param[0] == 1) ) {
				sta->bXCS = FALSE;						// �O����֕�����`�I��
				break;
			}
			break;

		default:
			break;
	}
	
	*srcpos = src;

	return;
}


void defaultMacroProc(const unsigned char c, ConvStatus *sta)
{
// �f�t�H���g�}�N���̏���
// �ԑg���(SI)�ł͎g�p����Ȃ�

	switch(c)
	{
		case 0x60:
			sta->bank[BANK_G0] = F_KANJI;
			sta->bank[BANK_G1] = F_ALPHA;
			sta->bank[BANK_G2] = F_HIRAGANA;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x61:
			sta->bank[BANK_G0] = F_KANJI;
			sta->bank[BANK_G1] = F_KATAKANA;
			sta->bank[BANK_G2] = F_HIRAGANA;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x62:
			sta->bank[BANK_G0] = F_KANJI;
			sta->bank[BANK_G1] = F_DRCS1A;
			sta->bank[BANK_G2] = F_HIRAGANA;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x63:
			sta->bank[BANK_G0] = F_MOSAICA;
			sta->bank[BANK_G1] = F_MOSAICC;
			sta->bank[BANK_G2] = F_MOSAICD;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x64:
			sta->bank[BANK_G0] = F_MOSAICA;
			sta->bank[BANK_G1] = F_MOSAICB;
			sta->bank[BANK_G2] = F_MOSAICD;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x65:
			sta->bank[BANK_G0] = F_MOSAICA;
			sta->bank[BANK_G1] = F_DRCS1A;
			sta->bank[BANK_G2] = F_MOSAICD;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x66:
			sta->bank[BANK_G0] = F_DRCS1A;
			sta->bank[BANK_G1] = F_DRCS2A;
			sta->bank[BANK_G2] = F_DRCS3A;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x67:
			sta->bank[BANK_G0] = F_DRCS4A;
			sta->bank[BANK_G1] = F_DRCS5A;
			sta->bank[BANK_G2] = F_DRCS6A;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x68:
			sta->bank[BANK_G0] = F_DRCS7A;
			sta->bank[BANK_G1] = F_DRCS8A;
			sta->bank[BANK_G2] = F_DRCS9A;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x69:
			sta->bank[BANK_G0] = F_DRCS10A;
			sta->bank[BANK_G1] = F_DRCS11A;
			sta->bank[BANK_G2] = F_DRCS12A;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x6a:
			sta->bank[BANK_G0] = F_DRCS13A;
			sta->bank[BANK_G1] = F_DRCS14A;
			sta->bank[BANK_G2] = F_DRCS15A;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x6b:
			sta->bank[BANK_G0] = F_KANJI;
			sta->bank[BANK_G1] = F_DRCS2A;
			sta->bank[BANK_G2] = F_HIRAGANA;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x6c:
			sta->bank[BANK_G0] = F_KANJI;
			sta->bank[BANK_G1] = F_DRCS3A;
			sta->bank[BANK_G2] = F_HIRAGANA;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x6d:
			sta->bank[BANK_G0] = F_KANJI;
			sta->bank[BANK_G1] = F_DRCS4A;
			sta->bank[BANK_G2] = F_HIRAGANA;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x6e:
			sta->bank[BANK_G0] = F_KATAKANA;
			sta->bank[BANK_G1] = F_HIRAGANA;
			sta->bank[BANK_G2] = F_ALPHA;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		case 0x6f:
			sta->bank[BANK_G0] = F_ALPHA;
			sta->bank[BANK_G1] = F_MOSAICA;
			sta->bank[BANK_G2] = F_DRCS1A;
			sta->bank[BANK_G3] = F_MACROA;
			sta->region[REGION_GL] = BANK_G0;
			sta->region[REGION_GR] = BANK_G2;
			break;
		default:
			break;
	}

	return;
}


int charclass(const WCHAR *sbuf, int *jiscode, int *slen)
{
//		unicode������𔻒肷��
//		
//		sbuf		���f���镶��buf
//		�߂�l		���f���ʂ̕�����(F_CONTROL, F_ALPHA, �c)
//		jiscode		���ʂ�jiscode
//		slen		���ʂ�unicode������(WCHAR�P��)

	int		len = 1;
	int		uc = sbuf[0];

	if( (uc >= 0xD800) && (uc <= 0xDBFF) ) {		// �T���Q�[�g�y�A�H
		int uc2 = sbuf[1];
		uc = ((uc - 0xD800) << 10) + (uc2 - 0xDC00) + 0x10000;
		len = 2;
	}


	// ����R�[�h (SP�܂�)

	if(uc <= 0x20) {
		*jiscode = uc;
		*slen = len;
		return F_CONTROL;
	}

	int		code;
	int		templen;


	// '['����n�܂�ǉ��L���W��

	if(uc == L'[') {
		templen = tuikakigou2revconv(sbuf, &code);
		if(templen != 0) {
			*jiscode = code;
			*slen = templen;
			return F_TUIKAKIGOU;
		}
	}


	// �p���W��

	if( (uc >= 0x21) && (uc <= 0x7E) ) {
		*jiscode = uc;
		*slen = len;
		return F_ALPHA;
	}


	// ��O�����񊿎���unicode���������ɊY���������

	templen = jis3combrevconv(sbuf, &code);
	if(templen != 0) {
		*jiscode = code;
		*slen = templen;
		return F_JIS1KANJI;
	}	


	// �������W����Љ����W���̋��ʕ���

	code = kanacommon1conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_KANACOMMON;
	}


	// �������W��

	code = hiragana1conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_HIRAGANA;
	}


	// �Љ����W��

	code = katakana1conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_KATAKANA;
	}


	// JIS X0201 �Љ����W��

	code = hankaku1conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_HANKAKU;
	}


	// �ǉ��L���W��

	code = tuikakigou1conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_TUIKAKIGOU;
	}


	// ��ꥑ�񐅏������W�� (�����W��)

	code = jis12conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_KANJI;
	}


	// ��O���������W�� (JIS�݊�����1��)

	code = jis3conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_JIS1KANJI;
	}


	// ��l���������W�� (JIS�݊�����2��)

	code = jis4conv(uc, FALSE);
	if(code != 0) {
		*jiscode = code;
		*slen = len;
		return F_JIS2KANJI;
	}


	// ���̑��̕s���ȕ���

	*jiscode = 0;
	*slen = len;
	return F_UNKNOWN;
}


BOOL kanaChange(const WCHAR *sbuf, const int srcpos, const int total_length, const int fcode)
{
//	�ϊ��R�X�g(�ϊ����ʂ̃o�C�g��)���l������
//	REGION_GR�̐ݒ��BANK_G2(F_HIRAGANA), BANK_G3(F_KATAKANA)�ԂŐ؂�ւ��邩���f����

	int		src = srcpos;
	int		charlen;
	int		jiscode;
	int		nfcode = fcode;

	KanaSequence	seq;
	memset(&seq, 0, sizeof(seq));

	while(src < total_length) {

		int f = charclass(sbuf + src, &jiscode, &charlen);

		if( (f == F_HIRAGANA) || (f == F_KATAKANA) ) {
			if(f == nfcode) {
				seq.num[seq.count]++;
				if(seq.num[seq.count] == 5) break;
			} else {
				seq.count++;
				if(seq.count == 20) break;
				nfcode = (nfcode == F_HIRAGANA) ? F_KATAKANA : F_HIRAGANA;
				seq.num[seq.count]++;
			}
		}

		src += charlen;
	}

	if(seq.num[0] == 1) return FALSE;
	if(seq.num[0] >= 5) return TRUE;

/*
	// �����̐؂�ւ��܂ōl�����Ă̔��f
	// �Ȃ�ׂ��y�����f���K�v�Ȃ炱�����

	if(seq.count == 0) {
		if(seq.num[0] > 1) return TRUE;
		return FALSE;
	}

	if(seq.count == 1) {
		if( (seq.num[0] = 3) && (seq.num[1] == 1) ) return TRUE;
		return FALSE;
	}

	if(seq.count == 2) {
		if(seq.num[0] == 2) {
			if(seq.num[1] == 1) return TRUE;
			if( (seq.num[1] == 2) && (seq.num[2] > 1) ) return TRUE;
			return FALSE;
		}
		if(seq.num[1] <= 2) return TRUE;
		if( (seq.num[1] == 3) && (seq.num[2] >= 2) ) return TRUE;
		return FALSE;
	}

	return FALSE;
*/

	// �ő�20���̐؂�ւ��܂ōl�����Ă̔��f

	if(seq.count == 20) return FALSE;

	kanacostc(&seq, 0, 0);
	int		ccost = seq.mincost;		// REGION_GR��؂�ւ����ꍇ�̍Œ�R�X�g

	kanacostn(&seq, 0, 0);
	int		ncost = seq.mincost;		// REGION_GR��؂�ւ��Ȃ��ꍇ�̍Œ�R�X�g

	if(ncost < ccost) return FALSE;

	return TRUE;
}


void kanacostc(KanaSequence *seq, const int count, const int cost)		// ���ȕϊ��R�X�g�v�Z�p
{
	int		newcost = cost + 2 + seq->num[count];
	if(count == seq->count) {
		if( (newcost <= seq->mincost) || (seq->mincost == 0) ) seq->mincost = newcost;
		return;
	}
	kanacostc(seq, count + 1, newcost);
	kanacostn(seq, count + 1, newcost);
	return;
}


void kanacostn(KanaSequence *seq, const int count, const int cost)		// ���ȕϊ��R�X�g�v�Z�p
{
	int		newcost = cost + 2 * seq->num[count];
	if(count == seq->count) {
		if( (newcost <= seq->mincost) || (seq->mincost == 0) ) seq->mincost = newcost;
		return;
	}
	kanacostp(seq, count + 1, newcost);
	return;
}


void kanacostp(KanaSequence *seq, const int count, const int cost)		// ���ȕϊ��R�X�g�v�Z�p
{
	int		newcost = cost + seq->num[count];
	if(count == seq->count) {
		if( (newcost <= seq->mincost) || (seq->mincost == 0) ) seq->mincost = newcost;
		return;
	}
	kanacostc(seq, count + 1, newcost);
	kanacostn(seq, count + 1, newcost);
	return;
}


BOOL kigouChange(const WCHAR *sbuf, const int srcpos, const int total_length)
{
//	sbuf�̌��݂̏ꏊsrcpos������F_TUIKAKIGOU, F_CONTROL, F_ALPHA�ȊO�̕������������TRUE��Ԃ�

	int		src = srcpos;
	int		charlen;
	int		jiscode;

	BOOL	bKigouOnly = TRUE;

	while(src < total_length) {

		int f = charclass(sbuf + src, &jiscode, &charlen);
		if( (f != F_TUIKAKIGOU) && (f != F_CONTROL) && (f != F_ALPHA) ) {
			bKigouOnly = FALSE;
			break;
		}

		src += charlen;
	}

	return bKigouOnly;
}


BOOL bankChange(const WCHAR *sbuf, const int srcpos, const int total_length, const ConvStatus *convsta, const int fcode)
{
//	�ϊ��R�X�g(�ϊ����ʂ̃o�C�g��)���l������
//	�g�������������BANK_G0, BANK_G1�̂ǂ���Ɋ���U��ׂ����𔻒f����
//
//	�߂�l	TRUE -> BANK_G0, FALSE -> BANK_G1

	int		src = srcpos;
	int		charlen;
	int		jiscode;
	int		nfcode = fcode;

	CharSequence	seq;
	memset(&seq, 0, sizeof(seq));

	seq.fcode[0] = nfcode;

	while(src < total_length) {

		int f = charclass(sbuf + src, &jiscode, &charlen);

		if( (f == F_ALPHA) || (f == F_HANKAKU) || (f == F_KANJI) || (f == F_JIS1KANJI) || (f == F_JIS2KANJI) || (f == F_TUIKAKIGOU) ) {
			if(f != nfcode) {
				if(seq.count == 255) break;
				seq.count++;
				nfcode = f;
				seq.fcode[seq.count] = nfcode;
			}
		}

		src += charlen;
	}

	BankStatus sta;
	memset(&sta, 0, sizeof(sta));

	sta.bankG0 = convsta->bank[BANK_G0];
	sta.bankG1 = convsta->bank[BANK_G1];
	sta.regionGL = convsta->region[REGION_GL];

	bankG0cost(&seq, sta, fcode);
	int		costG0 = seq.mincost;					// BANK_G0�Ɋ���U�����ꍇ�̍Œ�R�X�g

	seq.mincost = 0;
	bankG1cost(&seq, sta, fcode);
	int		costG1 = seq.mincost;					// BANK_G1�Ɋ���U�����ꍇ�̍Œ�R�X�g

	if(costG0 < costG1) return TRUE;
	if(costG0 > costG1) return FALSE;

	if( (fcode == F_ALPHA) || (fcode == F_ALPHA) ) return FALSE;

	return TRUE;
}


void bankG0cost(CharSequence *seq, BankStatus sta, const int fcode)			// BANK�؂�ւ��R�X�g�v�Z�p
{
	switch(fcode)
	{
		case F_ALPHA:
		case F_HANKAKU:
			sta.cost += 3;				// ESC 28 [F]
			sta.bankG0 = fcode;
			break;
		case F_KANJI:
		case F_JIS1KANJI:
		case F_JIS2KANJI:
		case F_TUIKAKIGOU:
			sta.cost += 3;				// ESC 24 [F}
			sta.bankG0 = fcode;
			break;
		default:
			break;
	}

	while( (seq->fcode[sta.count] == sta.bankG0) || (seq->fcode[sta.count] == sta.bankG1) )
	{
		if( (seq->fcode[sta.count] == sta.bankG0) && (sta.regionGL != BANK_G0) ) {
			sta.cost++;						// LS0
			sta.regionGL = BANK_G0;
		}
		if( (seq->fcode[sta.count] == sta.bankG1) && (sta.regionGL != BANK_G1) ) {
			sta.cost++;						// LS1
			sta.regionGL = BANK_G1;
		}
		if(sta.count == seq->count) {
			if( (seq->mincost == 0) || (seq->mincost > sta.cost) ) {
				seq->mincost = sta.cost;
			}
			return;
		}
		sta.count++;
	}

	bankG0cost(seq, sta, seq->fcode[sta.count]);
	bankG1cost(seq, sta, seq->fcode[sta.count]);

	return;
}


void bankG1cost(CharSequence *seq, BankStatus sta, const int fcode)			// BANK�؂�ւ��R�X�g�v�Z�p
{
	switch(fcode)
	{
		case F_ALPHA:
		case F_HANKAKU:
			sta.cost += 3;				// ESC 29 [F]
			sta.bankG1 = fcode;
			break;
		case F_KANJI:
		case F_JIS1KANJI:
		case F_JIS2KANJI:
		case F_TUIKAKIGOU:
			sta.cost += 4;				// ESC 24 29 [F}
			sta.bankG1 = fcode;
			break;
		default:
			break;
	}

	while( (seq->fcode[sta.count] == sta.bankG0) || (seq->fcode[sta.count] == sta.bankG1) )
	{
		if( (seq->fcode[sta.count] == sta.bankG0) && (sta.regionGL != BANK_G0) ) {
			sta.cost++;						// LS0
			sta.regionGL = BANK_G0;
		}
		if( (seq->fcode[sta.count] == sta.bankG1) && (sta.regionGL != BANK_G1) ) {
			sta.cost++;						// LS1
			sta.regionGL = BANK_G1;
		}
		if(sta.count == seq->count) {
			if( (seq->mincost == 0) || (seq->mincost > sta.cost) ) {
				seq->mincost = sta.cost;
			}
			return;
		}
		sta.count++;
	}

	bankG0cost(seq, sta, seq->fcode[sta.count]);
	bankG1cost(seq, sta, seq->fcode[sta.count]);

	return;
}

