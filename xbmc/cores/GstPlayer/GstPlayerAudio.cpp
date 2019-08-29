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
#include "cores/AudioEngine/AEFactory.h"

#define AUDIO_DEV "/dev/dvb/adapter0/audio0"

CGstPlayerAudio::CGstPlayerAudio(CProcessInfo &processInfo): m_processInfo(processInfo)
{
	m_processInfo.ResetAudioCodecInfo();
#if 0
	CAEFactory::Suspend();
	/* wait for AE has completed suspended */
	XbmcThreads::EndTime timer(1000);
	while (!timer.IsTimePast() && !CAEFactory::IsSuspended())
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
	if (!CAEFactory::Resume())
	{
		CLog::Log(LOGFATAL, "CGstPlayerAudio::%s: Failed to restart AudioEngine after return from player",__FUNCTION__);
	}
#endif
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
