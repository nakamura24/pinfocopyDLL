#include "stdafx.h"
#include <stdio.h>
#include <windows.h>

#include "sitprocess.h"
#include "tsprocess.h"
#include "proginfo.h"

//
//
// SIT�����N���X
// SitProcess�̃����o��`

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
	// �I���W�i����SIT�ƒ񋟂����ԑg��񂩂�A�V����SIT���쐬����

	int		src	= 0;
	int		dst	= 0;

	sitbuf[0x00] = 0x00;											//�@pointerfield 0x00

	memcpy(sitbuf + 1, psibuf, 10);									//�@TableID 0x7F �` Transmission_info_loop_length �܂�

	int		section_length					= getSectionLength(psibuf);
	int		transmission_info_loop_length	= getLength(psibuf + 0x08);

	sitbuf[0x06] |= 0x01;											// �J�����g�l�N�X�g�w�� "1"
	sitbuf[0x07] = 0;												// �Z�N�V�����ԍ� 0
	sitbuf[0x08] = 0;												// �ŏI�Z�N�V�����ԍ� 0

	src += 10;
	dst += 11;


	// �`�����Ɋւ���L�q�q���[�v

	int		new_transmission_info_loop_length = 0;

	for(int i = 0; i < transmission_info_loop_length; )
	{
		int	tag	= psibuf[src];
		int	len	= psibuf[src + 1] + 2;

		if( ((tag == 0xC2) && bChNum) ||							// bChNum�w��Ȃ猳���炠�����l�b�g���[�N���ʋL�q�q�͏���
			((tag == 0xCD) && bChNum) ||							// bChNum�w��Ȃ猳���炠����TS���L�q�q�͏���
			((tag == 0xC3) && bProgTime) )							// bProgTime�w��Ȃ猳���炠�����p�[�V����TS�^�C���L�q�q�͏���
		{
			i	+= len;
			src += len;
		}
		else														// ��L�ȊO�̋L�q�q�͎c��
		{
			memcpy(sitbuf + dst, psibuf + src, len);

			if( (tag == 0xC3) && ( (sitbuf[dst + 0x0E] & 0x01) != 0) )	ptst_desc_pos = dst;		// ���ݎ�����L����p�[�V����TS�^�C���L�q�q���c���ꍇ

			i	+= len;
			src += len;
			dst += len;
			new_transmission_info_loop_length += len;
		}
	}


	// �L�q�q�̒ǉ�

	if(bChNum) {
		memcpy(sitbuf + dst, proginfo->networkIdentifier, proginfo->networkIdentifierLen);			// �V���ȃl�b�g���[�N���ʋL�q�q��ǉ�
		dst += proginfo->networkIdentifierLen;
		new_transmission_info_loop_length += proginfo->networkIdentifierLen;
	}

	if(bChNum) {
		memcpy(sitbuf + dst, proginfo->tsInformation, proginfo->tsInformationLen);					// �V����TS���L�q�q��ǉ�
		dst += proginfo->tsInformationLen;
		new_transmission_info_loop_length += proginfo->tsInformationLen;
	}


	if(new_transmission_info_loop_length >= 4096) return SIT_TRANSMISSION_INFO_LOOP_TOO_LONG;		// �V���ɍ쐬����transmission_info_loop����������ꍇ

	sitbuf[0x09] = (new_transmission_info_loop_length | 0xF000) >> 8;								// �V����transmission_info_loop_length�ɍX�V
	sitbuf[0x0A] = new_transmission_info_loop_length & 0x00FF;


	//service���Ƃ̃��[�v�iSIT�ł͒ʏ�͈�̃T�[�r�X�����H�j

	while( src < (section_length - 1) )
	{
		memcpy(sitbuf + dst, psibuf + src, 4);														// �T�[�r�XID �` �T�[�r�X���[�v���܂ŃR�s�[

		int		service_loop_length = getLength(psibuf + src + 2);

		if(bChNum) {
			sitbuf[dst + 0] = proginfo->serviceID >> 8;												// service_id�̕ύX
			sitbuf[dst + 1] = proginfo->serviceID & 0xFF;
		}

		src += 4;
		dst += 4;


		// �e�T�[�r�X�Ɋւ���L�q�q���[�v

		unsigned char	oldShortEvent[MAXDESCSIZE];
		memset(oldShortEvent, 0, sizeof(oldShortEvent));
		int		oldShortEventLen = 0;

		int		new_service_loop_length = 0;

		for(int i = 0; i < service_loop_length; )
		{
			int	tag	= psibuf[src];
			int	len	= psibuf[src + 1] + 2;

			if( ((tag == 0xD6) && bChNum) ||						// bChNum�w��Ȃ猳�̃C�x���g�O���[�v�L�q�q�͏���
				((tag == 0x85) && bChNum) ||						// bChNum�w��Ȃ猳�̕���ID�L�q�q�͏���
				((tag == 0xD8) && bChNum) ||						// bChNum�w��Ȃ猳�̃u���[�h�L���X�^���L�q�q�͏���
				((tag == 0xCE) && bChNum) ||						// bChNum�w��Ȃ猳�̊g���u���[�h�L���X�^�L�q�q�͏���
				((tag == 0xC3) && bProgTime) ||						// bProgTime�w��Ȃ猳�̃p�[�V����TS�^�C���L�q�q�͏���
				((tag == 0x48) && bChName) ||						// bChName�w��Ȃ猳�̃T�[�r�X�L�q�q�͏���
				((tag == 0x4D) && (bProgName || bProgDetail)) ||	// bProgName��������bProgDetail�w��Ȃ猳�̒Z�`���C�x���g�L�q�q�͏���
				((tag == 0x4E) && bProgExt) ||						// bProgExt�w��Ȃ猳�̊g���`���C�x���g�L�q�q�͏���
				((tag == 0x54) && bProgGenre) ||					// bProgGenre�w��Ȃ猳�̃R���e���g�L�q�q�͏���
				((tag == 0x50) && bVComponent) ||					// bVComponent�w��Ȃ猳�̃R���|�[�l���g�L�q�q�͏���
				((tag == 0xC4) && bAComponent) )					// bAComponent�w��Ȃ猳�̉����R���|�[�l���g�L�q�q�͏���
			{
				if( (tag == 0x4D) && (bProgName ^ bProgDetail) )	// �Z�`���C�x���g�L�q�q�������ꍇ�ł����Ă� ProgName/ProgDetail �̂ǂ��炩������c���K�v������ꍇ
				{
					memcpy(oldShortEvent, psibuf + src, len);		// ���̒Z�`���C�x���g�L�q�q��ۑ����Ă���
					oldShortEventLen = len;
				}

				i	+= len;
				src += len;
			}
			else													// ��L�ȊO�̋L�q�q�͎c��
			{
				memcpy(sitbuf + dst, psibuf + src, len);

				if(tag == 0xC3)										// �p�[�V����TS�^�C���L�q�q���c���ꍇ�A�ԑg�̊J�n������ۑ����Ă���
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


		// �L�q�q�̒ǉ�

		if( bProgTime && (proginfo->partialTsTimeLen != 0) ) {
			memcpy(sitbuf + dst, proginfo->partialTsTime, proginfo->partialTsTimeLen);				// �V���ȃp�[�V����TS�^�C���L�q�q�̒ǉ�
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
			memcpy(sitbuf + dst, proginfo->component, proginfo->componentLen);						// �V���ȃR���|�[�l���g�L�q�q�̒ǉ�
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
					memcpy(sitbuf + dst, proginfo->audioComponent + i, len);						// �V���ȉ����R���|�[�l���g�L�q�q�̒ǉ�
					dst += len;
					new_service_loop_length += len;
				}
				i += len;
				count++;
			}
		}

		if(bChNum) {
			memcpy(sitbuf + dst, proginfo->extendedBroadcaster, proginfo->extendedBroadcasterLen);		// �V���Ȋg���u���[�h�L���X�^�L�q�q�̒ǉ�
			dst += proginfo->extendedBroadcasterLen;
			new_service_loop_length += proginfo->extendedBroadcasterLen;
		}

		if(bChNum) {
			memcpy(sitbuf + dst, proginfo->broadcasterName, proginfo->broadcasterNameLen);			// �V���ȃu���[�h�L���X�^���L�q�q�̒ǉ�
			dst += proginfo->broadcasterNameLen;
			new_service_loop_length += proginfo->broadcasterNameLen;
		}

		if(bChNum) {
			memcpy(sitbuf + dst, proginfo->eventGroup, proginfo->eventGroupLen);					// �V���ȃC�x���g�O���[�v�L�q�q�̒ǉ�
			dst += proginfo->eventGroupLen;
			new_service_loop_length += proginfo->eventGroupLen;
		}

		if(bChName) {
			memcpy(sitbuf + dst, proginfo->service, proginfo->serviceLen);							// �V���ȃT�[�r�X�L�q�q�̒ǉ�
			dst += proginfo->serviceLen;
			new_service_loop_length += proginfo->serviceLen;
		}

		if(bProgName && bProgDetail) {
			memcpy(sitbuf + dst, proginfo->shortEvent, proginfo->shortEventLen);					// �V���ȒZ�`���C�x���g�L�q�q�̒ǉ�
			dst += proginfo->shortEventLen;
			new_service_loop_length += proginfo->shortEventLen;
		}

		if(bProgName ^ bProgDetail) {																// ���̒Z�`���C�x���g�L�q�q�ƐV���ȒZ�`���C�x���g�L�q�q���ꏏ�ɂ��č�蒼���A�����ǉ�����
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

			if(tmpDescriptorLen == 7) tmpDescriptorLen = 0;											// ���g����̒Z�`���C�x���g�L�q�q�ɂȂ����ꍇ�͑��݂��Ȃ����̂Ƃ��ăR�s�[���Ȃ�

			memcpy(sitbuf + dst, tmpDescriptor, tmpDescriptorLen);									// �V���ɍ쐬���������Z�`���C�x���g�L�q�q�̒ǉ�
			dst += tmpDescriptorLen;
			new_service_loop_length += tmpDescriptorLen;
		}

		if(bProgExt) {
			memcpy(sitbuf + dst, proginfo->extendedEvent, proginfo->extendedEventLen);				// �V���Ȋg���`���C�x���g�L�q�q�̒ǉ�
			dst += proginfo->extendedEventLen;
			new_service_loop_length += proginfo->extendedEventLen;
		}

		if(bProgGenre) {
			memcpy(sitbuf + dst, proginfo->content, proginfo->contentLen);							// �V���ȃR���e���g�L�q�q�̒ǉ�
			dst += proginfo->contentLen;
			new_service_loop_length += proginfo->contentLen;
		}

		if(new_service_loop_length > 4096) return SIT_SERVICE_LOOP_TOO_LONG;

		sitbuf[dst - new_service_loop_length - 2] = (new_service_loop_length | 0x8000) >> 8;		// �V����service_loop_length�ɍX�V
		sitbuf[dst - new_service_loop_length - 1] = new_service_loop_length & 0xFF;
	
	}

	if(dst > 4093) return SIT_SECTION_TOO_LONG;														// section�� > 4093 �Ȃ�s�K

	sitbuf[0x02] &= 0xF0;																			// �V����section_length�ɍX�V
	sitbuf[0x02] |= (dst >> 8);
	sitbuf[0x03] = dst & 0xFF;


	// CRC�Čv�Z

	unsigned int	crc = calc_crc32(sitbuf + 1, dst - 1);

	sitbuf[dst + 0] = (crc >> 24) & 0xFF;
	sitbuf[dst + 1] = (crc >> 16) & 0xFF;
	sitbuf[dst + 2] = (crc >>  8) & 0xFF;
	sitbuf[dst + 3] = (crc >>  0) & 0xFF;

	memset(sitbuf + dst + 4, 0xFF, sizeof(sitbuf) - dst - 4);										// �c���0xFF�Ŗ��߂�


	// ���̑�

	buflen	= dst + 4;

	sitnum = buflen / 184;																			// �f�[�^�����p�P�b�g�����v�Z
	if( (buflen % 184) != 0 ) sitnum++;


	return SIT_MAKE_NEW_SIT_SUCCESS;
}

	
int SitProcess::getPacket(const unsigned char *packet, unsigned char *wbuf)
{

	// PCR����̌o�ߎ��ԏ��̎擾�Cptst_desc_pos != -1 �Ȃ珑��������ׂ��p�[�V����TS�^�C���L�q�q�����݂���

	if( (ptst_desc_pos != -1) && (getPid(packet) == pcrpid) && isPcrData(packet) )
	{
		__int64		new_pcr = getPcrValue(packet);																		// PCR�l�擾

		if(!bPcrStarted) {
			prev_pcr = new_pcr;																							// �ŏ���PCR�l�̏ꍇ
			bPcrStarted = TRUE;
		}

		__int64		pcr_interval = (new_pcr >= prev_pcr) ? new_pcr - prev_pcr : new_pcr + 2576980377600 - prev_pcr;		// PCR�l�̊Ԋu�i�o�ߎ��ԁj�̌v�Z
		if(pcr_interval > SIT_MAX_PCR_TIME_INTERVAL) pcr_interval = SIT_MAX_PCR_TIME_INTERVAL;							// ���炩�̗��R��PCR�l�̊Ԋu���L������ꍇ�A���l�ɗ}����

		pcr_counter += pcr_interval;																					// �g�[�^���̌o�ߎ��ԃJ�E���^
		prev_pcr = new_pcr;
	}


	// SIT�p�P�b�g�Ɋւ���u������

	if(getPid(packet) != pid) return SIT_NOT_SIT_PACKET;											// SIT�p�P�b�g�ł͂Ȃ������ꍇ


	// �p�[�V����TS�^�C���L�q�q�̌��ݎ����C��

	if( (ptst_desc_pos != -1) && (sitcount == 0) )
	{
		// ���ݎ����v�Z�D�t�@�C���̐擪��ԑg�J�n�����Ƃ��A���̌��PCR�ɏ]���Ď��Ԍo��

		int	sec		= time_sec + (int)(pcr_counter / 27000000);

		int	newmjd	= time_mjd + (sec / 86400);
		int	newhour	= (sec % 86400) / 3600;
		int	newmin	= (sec % 3600) / 60;
		int	newsec	= sec % 60;

		// ������������

		sitbuf[ptst_desc_pos + 0x0F] = newmjd >> 8;
		sitbuf[ptst_desc_pos + 0x10] = newmjd & 0x00FF;
		sitbuf[ptst_desc_pos + 0x11] = (newhour / 10) * 16 + (newhour % 10);
		sitbuf[ptst_desc_pos + 0x12] = (newmin / 10) * 16 + (newmin % 10);
		sitbuf[ptst_desc_pos + 0x13] = (newsec / 10) * 16 + (newsec % 10);

		if(sec != time_sec_old) version_number++;												// ������񂪍X�V���ꂽ��SIT�̃o�[�W�����ԍ����グ��
		if(version_number == 32) version_number = 0;
		time_sec_old = sec;
		sitbuf[0x06] = (version_number << 1) | 0xC1;											// �o�[�W�����ԍ���������

		// CRC�Čv�Z���ďC��

		unsigned int	crc = calc_crc32(sitbuf + 1, buflen - 5);

		sitbuf[buflen - 4] = (crc >> 24) & 0xFF;
		sitbuf[buflen - 3] = (crc >> 16) & 0xFF;
		sitbuf[buflen - 2] = (crc >>  8) & 0xFF;
		sitbuf[buflen - 1] = (crc >>  0) & 0xFF;
	}


	// �ϊ��ς݂�SIT��1�p�P�b�g���Ƃɏo��

	memcpy(wbuf + 4, sitbuf + sitcount * 184, 184);												// �o�̓o�b�t�@�ɃR�s�[

	if(sitcount == 0) {
		*(wbuf + 1) |= 0x40;																	// payload_unit_start_indicator���Z�b�g����
	} else {
		*(wbuf + 1) &= 0xBF;																	// payload_unit_start_indicator�����Z�b�g����
	}

	*(wbuf + 3) &= 0xDF;																		// adaptation_filed����
	*(wbuf + 3) |= 0x10;																		// payload����

	sitcount++;
	if(sitcount == sitnum) sitcount = 0;

	return SIT_REPLACEMENT_SUCCESS;
}




	





