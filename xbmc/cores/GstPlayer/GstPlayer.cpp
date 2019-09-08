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

#include "GstPlayer.h"
#include "GstPlayerVideo.h"
#include "GstPlayerAudio.h"

#include "system.h"
#include "Util.h"
#include "FileItem.h"
#include "Application.h"
#include "ServiceBroker.h"
#include "CompileInfo.h"
#include "GUIInfoManager.h"
#include "URL.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"
#include "cores/DataCacheCore.h"
#include "input/InputManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/DisplaySettings.h"
#include "threads/SystemClock.h"
#include "threads/SingleLock.h"
#include "windowing/WinSystem.h"
#include "guilib/GUIWindowManager.h"
#include "filesystem/MusicDatabaseFile.h"
#include "dialogs/GUIDialogBusy.h"
#include "pvr/PVRManager.h"

#include "cores/VideoPlayer/Process/ProcessInfo.h"

#include "cores/VideoPlayer/VideoPlayer.h"

#include "ServiceBroker.h"
#include "cores/AudioEngine/Engines/ActiveAE/ActiveAE.h"

#include "cores/VideoPlayer/DVDInputStreams/DVDFactoryInputStream.h"
#include "DVDInputStreams/InputStreamPVRBase.h"
#include "settings/MediaSettings.h"
#include "storage/MediaManager.h"

//using namespace PVR;
using namespace XFILE;

// Default time after which the item's playcount is incremented
#define DEFAULT_PLAYCOUNT_MIN_TIME 10

CGstPlayer::CGstPlayer(IPlayerCallback& callback)
	: IPlayer(callback)
	,CThread("GstPlayer")
	,m_bAbortRequest(false)
	,m_isRecording(false)
	,m_playbackStartTime(0)
	,m_playCountMinTime(DEFAULT_PLAYCOUNT_MIN_TIME)
	,m_speed(1)
	,m_totalTime(1)
	,m_time(0)
	,m_ready(true)
//	,m_messenger("player")
{
	m_pInputStream = nullptr;
	m_canTempo = false;
	m_processInfo.reset(CProcessInfo::CreateInstance());
	m_processInfo->SetSpeed(1.0);
	m_processInfo->SetTempo(1.0);
	
	CServiceBroker::GetActiveAE()->Suspend();
	  
	while (!CServiceBroker::GetActiveAE()->IsSuspended())
		Sleep(10);
	
	CreatePlayers();
}

void CGstPlayer::CreatePlayers()
{
	m_VideoPlayerAudio = new CGstPlayerAudio(*m_processInfo);
	m_VideoPlayerVideo = new CGstPlayerVideo(this, *m_processInfo);
}

void CGstPlayer::DestroyPlayers()
{
	delete m_VideoPlayerVideo;
	delete m_VideoPlayerAudio;
}

CGstPlayer::~CGstPlayer()
{
	CloseFile();
	
	m_pInputStream.reset();
	
	DestroyPlayers();
	
	CServiceBroker::GetActiveAE()->Resume();
	
	CLog::Log(LOGNOTICE, "CGstPlayer::%s", __FUNCTION__ );
}

bool CGstPlayer::OpenFile(const CFileItem& file, const CPlayerOptions &options)
{
	if (IsPlaying())
		CloseFile();
	
	m_bAbortRequest = false;
	m_processInfo->SetSpeed(1.0);
	m_processInfo->SetTempo(1.0);
	m_PlayerOptions = options;
	m_item = file;
	// Try to resolve the correct mime type
	m_item.SetMimeTypeForInternetFile();
	m_ready.Reset();
	
	Create();
	
	return true;
}

bool CGstPlayer::CloseFile(bool reopen)
{
	m_bAbortRequest = true;
	
	m_VideoPlayerVideo->CloseStream();
	
	if(m_pInputStream)
		m_pInputStream->Abort();
	
	/* wait for the thread to terminate */
	StopThread(true);
	
	CLog::Log(LOGNOTICE, "CGstPlayer::%s: reopen=%s", __FUNCTION__, reopen == true ? "true":"false");

	return true;
}

bool CGstPlayer::OpenInputStream()
{
	if (m_pInputStream.use_count() > 1)
		throw std::runtime_error("m_pInputStream reference count is greater than 1");
	m_pInputStream.reset();

	CLog::Log(LOGNOTICE, "Creating InputStream");

	// correct the filename if needed
	std::string filename(m_item.GetPath());
	if (URIUtils::IsProtocol(filename, "dvd") ||
			StringUtils::EqualsNoCase(filename, "iso9660://video_ts/video_ts.ifo"))
	{
		m_item.SetPath(g_mediaManager.TranslateDevicePath(""));
	}

	m_pInputStream = CDVDFactoryInputStream::CreateInputStream(this, m_item, true);
	if(m_pInputStream == NULL)
	{
		CLog::Log(LOGERROR, "CGstPlayer%s: - unable to create input stream for [%s]", __FUNCTION__, CURL::GetRedacted(m_item.GetPath()).c_str());
		return false;
	}

	if (!m_pInputStream->Open())
	{
		CLog::Log(LOGERROR, "CGstPlayer%s: - error opening [%s]", __FUNCTION__, CURL::GetRedacted(m_item.GetPath()).c_str());
		return false;
	}

	// find any available external subtitles for non dvd files
	if (!m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD)
	&&	!m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER)
	&&	!m_pInputStream->IsStreamType(DVDSTREAM_TYPE_TV))
	{
		// find any available external subtitles
		std::vector<std::string> filenames;
		CUtil::ScanForExternalSubtitles(m_item.GetPath(), filenames);

		// load any subtitles from file item
		std::string key("subtitle:1");
		for(unsigned s = 1; m_item.HasProperty(key); key = StringUtils::Format("subtitle:%u", ++s))
			filenames.push_back(m_item.GetProperty(key).asString());

		for(unsigned int i=0;i<filenames.size();i++)
		{
			// if vobsub subtitle:
			if (URIUtils::HasExtension(filenames[i], ".idx"))
			{
				std::string strSubFile;
				if ( CUtil::FindVobSubPair( filenames, filenames[i], strSubFile ) )
				{
					CLog::Log(LOGNOTICE, "CGstPlayer%s: - AddSubtitleFile", __FUNCTION__);
					//AddSubtitleFile(filenames[i], strSubFile);
				}
			}
			else
			{
				if ( !CUtil::IsVobSub(filenames, filenames[i] ) )
				{
					CLog::Log(LOGNOTICE, "CGstPlayer%s: - AddSubtitleFile", __FUNCTION__);
					//AddSubtitleFile(filenames[i]);
				}
			}
		} // end loop over all subtitle files
	}

	//m_clock.Reset();
	//m_dvd.Clear();
	//m_errorCount = 0;
	//m_ChannelEntryTimeOut.SetInfinite();
	
	if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_FILE))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_FILE", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_DVD))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_DVD", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_HTTP))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_HTTP", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_MEMORY))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_MEMORY", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_FFMPEG))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_FFMPEG", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_TV))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_TV", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_MPLS))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_MPLS", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_BLURAY))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_BLURAY", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_PVRMANAGER", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_MULTIFILES))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_MULTIFILES", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_ADDON))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_ADDON", __FUNCTION__);
	else if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_NONE))
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_NONE", __FUNCTION__);
	else 
		CLog::Log(LOGNOTICE, "CGstPlayer%s: DVDSTREAM_TYPE_UNKNOWN", __FUNCTION__);
	
	CLog::Log(LOGNOTICE, "CGstPlayer%s: m_ready.Set()", __FUNCTION__);
	m_ready.Set();

	bool realtime = m_pInputStream->IsRealtime();

	if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_VIDEOPLAYER_USEDISPLAYASCLOCK) &&
			!realtime)
		m_canTempo = true;
	else
		m_canTempo = false;

	return true;
}

void CGstPlayer::Process()
{
	if (!OpenInputStream())
	{
		m_bAbortRequest = true;
		return;
	}
	
	CDVDStreamInfo hint;
	
	hint.Clear();
	hint.filename = m_pInputStream->GetFileName();
	
	CURL Url(hint.filename);
	
	CLog::Log(LOGNOTICE, "CGstPlayer%s: m_pInputStream->GetFileName(): %s", __FUNCTION__, hint.filename.c_str());

	m_playbackStartTime = XbmcThreads::SystemClockMillis();
	m_time = 0;
	
	//m_callback.OnPlayBackStarted();

	if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER) && 
		Url.IsLocalHost())
	{
		hint.filename = "";
	}
	
	m_VideoPlayerVideo->OpenStream(hint, m_item);
	
	CLog::Log(LOGNOTICE, "CGstPlayer%s: m_bStop: %s", __FUNCTION__, m_bStop == true ? "true" : "false");
	
	m_bStop = true;
	
	CLog::Log(LOGNOTICE, "CGstPlayer%s: m_bStop: %s", __FUNCTION__, m_bStop == true ? "true" : "false");

	// We don't want to come back to an active screensaver
	g_application.ResetScreenSaver();
	g_application.WakeUpScreenSaverAndDPMS();

	if (g_application.CurrentFileItem().IsStack() || 
			m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER))
		m_callback.OnPlayBackStopped();
	else
		m_callback.OnPlayBackEnded();
}

void CGstPlayer::Pause()
{
	m_VideoPlayerVideo->Pause();
}

bool CGstPlayer::HasVideo() const
{
	return true;
}

bool CGstPlayer::HasAudio() const
{
	return false;
}

void CGstPlayer::SwitchToNextLanguage()
{
	m_VideoPlayerVideo->SwitchToNextLanguage();
}

void CGstPlayer::ToggleSubtitles()
{
	m_VideoPlayerVideo->ToggleSubtitles();
}

bool CGstPlayer::CanSeek()
{
	return m_VideoPlayerVideo->CanSeek();
}

void CGstPlayer::Seek(bool bPlus, bool bLargeStep, bool bChapterOverride)
{
	m_VideoPlayerVideo->Seek(bPlus, bLargeStep, bChapterOverride);
}

void CGstPlayer::SwitchToNextAudioLanguage()
{
	m_VideoPlayerVideo->SwitchToNextAudioLanguage();
}

void CGstPlayer::SeekPercentage(float iPercent)
{
	m_VideoPlayerVideo->SeekPercentage(iPercent);
}

float CGstPlayer::GetPercentage()
{
	int64_t iTime = GetTime();
	int64_t iTotalTime = GetTotalTime();

	if (iTotalTime != 0)
	{
		//CLog::Log(LOGNOTICE, "CGstPlayer::%s: Percentage=%f", __FUNCTION__ , (iTime * 100 / (float)iTotalTime) );
		return iTime * 100 / (float)iTotalTime;
	}

	return 0.0f;
}

void CGstPlayer::SetAVDelay(float fValue)
{
	m_VideoPlayerVideo->SetAVDelay(fValue);
}

float CGstPlayer::GetAVDelay()
{
	return m_VideoPlayerVideo->GetAVDelay();
}

void CGstPlayer::SetSubTitleDelay(float fValue)
{
	m_VideoPlayerVideo->SetSubTitleDelay(fValue);
}

float CGstPlayer::GetSubTitleDelay()
{
	return m_VideoPlayerVideo->GetSubTitleDelay();
}

void CGstPlayer::SeekTime(int64_t iTime)
{
	int seekOffset = (int)(iTime - GetTime());
	
	m_VideoPlayerVideo->SeekTime(iTime);
	m_callback.OnPlayBackSeek((int)iTime, seekOffset);
}

bool CGstPlayer::SeekTimeRelative(int64_t iTime)
{
	bool res = m_VideoPlayerVideo->SeekTimeRelative(iTime);
	
	if(res)
	{
		int64_t abstime = GetTime() + iTime;
		m_callback.OnPlayBackSeek((int)abstime, iTime);
	}
	
	return res;
}

int64_t CGstPlayer::GetTime() // in milliseconds
{
	return m_VideoPlayerVideo->GetTime();
}

int64_t CGstPlayer::GetTotalTime() // in milliseconds
{
	return m_VideoPlayerVideo->GetTotalTime();
}

bool CGstPlayer::CanRecord()
{
	return false;
}

bool CGstPlayer::IsRecording()
{
	return false;
}

void CGstPlayer::SetSpeed(float iSpeed)
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s: %f", __FUNCTION__, iSpeed);
}

void CGstPlayer::ShowOSD(bool bOnoff)
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s: %s", __FUNCTION__, bOnoff == false ? "false":"true" );
}

std::string CGstPlayer::GetPlayerState()
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s", __FUNCTION__);
	return "";
}

bool CGstPlayer::SetPlayerState(const std::string& state)
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s: state:%s", __FUNCTION__, state.c_str());
	return true;
}

bool CGstPlayer::Initialize(TiXmlElement* pConfig)
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s", __FUNCTION__);
	return true;
}

void CGstPlayer::Render(bool clear, uint32_t alpha, bool gui)
{
	//CServiceBroker::GetWinSystem().ClearBuffers(0.0f);

	//CLog::Log(LOGNOTICE, "CGstPlayer::%s: clear=%s, alpha=%d, gui=%s", __FUNCTION__, clear == false ? "false":"true", (int)alpha, gui == false ? "false":"true");
}

void CGstPlayer::ToggleOSD()
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s:", __FUNCTION__);
}

void CGstPlayer::SetVolume(float volume)
{
	m_VideoPlayerAudio->SetVolume(volume);
}

void CGstPlayer::SetMute(bool bOnOff)
{
	m_VideoPlayerAudio->SetMute(bOnOff);
}

int CGstPlayer::GetAudioStreamCount()
{
	return m_VideoPlayerVideo->GetAudioStreamCount();
}

int CGstPlayer::GetVideoStreamCount()
{
	return m_VideoPlayerVideo->GetVideoStreamCount();
}

int CGstPlayer::GetSubtitleCount()
{
	return m_VideoPlayerVideo->GetSubtitleCount();
}

int CGstPlayer::GetAudioStream()
{
	return m_VideoPlayerVideo->GetAudioStream();
}

int CGstPlayer::GetVideoStream()
{
	return m_VideoPlayerVideo->GetVideoStream();
}

int CGstPlayer::GetSubtitle()
{
	return m_VideoPlayerVideo->GetSubtitle();
}

float CGstPlayer::GetRenderAspectRatio()
{
	return m_VideoPlayerVideo->GetRenderAspectRatio();
}

void CGstPlayer::OnPlaybackStarted()
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s", __FUNCTION__);
	
	CURL Url(m_pInputStream->GetFileName());
	
	CLog::Log(LOGNOTICE, "CGstPlayer::%s: startpercent=%f starttime=%f", __FUNCTION__, m_PlayerOptions.startpercent, m_PlayerOptions.starttime);
	
	if (m_PlayerOptions.startpercent > 0)
	{
		//CLog::Log(LOGNOTICE, "CGstPlayer::%s: m_PlayerOptions.startpercent set", __FUNCTION__);
		m_VideoPlayerVideo->SeekPercentage(m_PlayerOptions.startpercent/(float) 100);
		m_PlayerOptions.startpercent = 0.0f;
	}
	else if (m_PlayerOptions.starttime > 0)
	{
		//CLog::Log(LOGNOTICE, "CGstPlayer::%s: m_PlayerOptions.starttime set", __FUNCTION__);
		m_VideoPlayerVideo->SeekTimeRelative((int64_t) m_PlayerOptions.starttime * 1000);
		m_PlayerOptions.starttime = 0.0f;
	}
	
	if (m_pInputStream->IsStreamType(DVDSTREAM_TYPE_PVRMANAGER) && 
		//Url.IsLocalHost() && !IsRecording())
		Url.IsLocalHost() && !CServiceBroker::GetPVRManager().IsRecording())
	{
		FILE *f = fopen("/proc/stb/fp/led0_pattern", "w");
		if (f)
		{
			if (fprintf(f, "%08x", 0xffffffff) == 0)
				CLog::Log(LOGNOTICE, "CGstPlayer::%s: switch off fake record LED", __FUNCTION__);
			fclose(f);
		}
	}
	m_callback.OnPlayBackStarted(m_item);
	//m_ready.Set();
}

bool CGstPlayer::OnAction(const CAction &action)
{
	CLog::Log(LOGNOTICE, "CGstPlayer::%s: id=%d", __FUNCTION__, action.GetID());
	return false;
}

void CGstPlayer::GetVideoResolution(unsigned int &width, unsigned int &height)
{
	RESOLUTION_INFO res = CServiceBroker::GetWinSystem()->GetGfxContext().GetResInfo();
	width = res.iWidth;
	height = res.iHeight;
	
	CLog::Log(LOGNOTICE, "CGstPlayer::%s: width=%d height=%d", __FUNCTION__, width, height);
}

void CGstPlayer::FrameMove() 
{ 
	//CLog::Log(LOGNOTICE, "CGstPlayer::%s", __FUNCTION__ );
}

bool CGstPlayer::IsPlaying() const
{
  return !m_bStop;
}
