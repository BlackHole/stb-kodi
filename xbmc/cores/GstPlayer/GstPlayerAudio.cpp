/*
 *      Copyright (C) 2005-2015 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "threads/SingleLock.h"
#include "utils/XMLUtils.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dvb/audio.h>

#include "system.h"
#include "GstPlayerAudio.h"
#include "utils/log.h"

#include "threads/Thread.h"
#include "threads/SystemClock.h"
#include "ServiceBroker.h"
#include "cores/AudioEngine/Engines/ActiveAE/ActiveAE.h"

#define AUDIO_DEV "/dev/dvb/adapter0/audio0"

CGstPlayerAudio::CGstPlayerAudio(CProcessInfo &processInfo): m_processInfo(processInfo)
{
	CLog::Log(LOGNOTICE, "CGstPlayerAudio::%s", __FUNCTION__ );
	// magic value used when omxplayer is playing - want sink to be disabled
	AEAudioFormat m_format;
	m_format.m_dataFormat = AE_FMT_RAW;
	m_format.m_streamInfo.m_type = CAEStreamInfo::STREAM_TYPE_AC3;
	m_format.m_streamInfo.m_sampleRate = 16000;
	m_format.m_streamInfo.m_channels = 2;
	m_format.m_sampleRate = 16000;
	m_format.m_frameSize = 1;
	m_pAudioStream = CServiceBroker::GetActiveAE()->MakeStream(m_format);

	m_processInfo.ResetAudioCodecInfo();
#if 0
	CServiceBroker::GetActiveAE()->Suspend();
	/* wait for AE has completed suspended */
	XbmcThreads::EndTime timer(1000);
	while (!timer.IsTimePast() && !CServiceBroker::GetActiveAE()->IsSuspended())
	{
		usleep(80);
	}
	if (timer.IsTimePast())
	{
		CLog::Log(LOGERROR,"CGstPlayerAudio::%s: AudioEngine did not suspend before launching player", __FUNCTION__);
	}
#endif
}

CGstPlayerAudio::~CGstPlayerAudio()
{
#if 0
	/* Resume AE processing of XBMC native audio */
	if (!CServiceBroker::GetActiveAE()->Resume())
	{
		CLog::Log(LOGFATAL, "CGstPlayerAudio::%s: Failed to restart AudioEngine after return from player",__FUNCTION__);
	}
#endif
	if (m_pAudioStream)
		CServiceBroker::GetActiveAE()->FreeStream(m_pAudioStream, true);
	CLog::Log(LOGNOTICE, "CGstPlayerAudio::%s", __FUNCTION__ );
}

void CGstPlayerAudio::SetVolume(float volume)
{
	int vol = checkVolume(int(volume * 100));
	audio_mixer_t mixer;
	
	vol = 63 - vol * 63 / 100;
	
	mixer.volume_left = vol;
	mixer.volume_right = vol;

	int fd = openMixer();
	if (fd >= 0)
	{
		ioctl(fd, AUDIO_SET_MIXER, &mixer);
		closeMixer(fd);
	}
	CLog::Log(LOGDEBUG, "%s: volume:%f set:%d (-1db)", __FUNCTION__, volume, vol);
}

void CGstPlayerAudio::SetMute(bool bOnOff)
{
	int fd = openMixer();
	if (fd >= 0)
	{
		bool mute = bOnOff; //!m_muted;
		if (!ioctl(fd, AUDIO_SET_MUTE, mute) == 0)
			CLog::Log(LOGERROR, "%s: ioctl AUDIO_SET_MUTE failed", __FUNCTION__);
		
		closeMixer(fd);
	}
	else
			CLog::Log(LOGERROR, "%s: i/o AUDIO_SET_MUTE failed", __FUNCTION__);
	
	CLog::Log(LOGDEBUG, "%s: bOnOff	:%s", __FUNCTION__, bOnOff == false ? "off":"on" ); 
}

int CGstPlayerAudio::openMixer()
{
	return open(AUDIO_DEV, O_RDWR);
}

void CGstPlayerAudio::closeMixer(int fd)
{
	if (fd >= 0) close(fd);
}

int CGstPlayerAudio::checkVolume(int vol)
{
	if (vol < 0)
		vol = 0;
	else if (vol > 100)
		vol = 100;
	return vol;
}
