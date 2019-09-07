#pragma once

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

#include <string>
#include "system.h"

#include "GstPlayerAudio.h"
#include "GstPlayerVideo.h"
#include "cores/IPlayer.h"
#include "threads/Thread.h"
#include "utils/log.h"
#include "FileItem.h"

#include "cores/VideoPlayer/IVideoPlayer.h"
#include "cores/VideoPlayer/DVDInputStreams/DVDInputStream.h"

class CProcessInfo;
class CGstPlayerAudio;
class CGstPlayerVideo;
class CInputStreamPVRManager;

using namespace PVR;

class IGstPlayerCallback
{
public:
	virtual void OnPlaybackStarted() = 0;
};

class CGstPlayer : public IPlayer, public CThread, public IVideoPlayer, public IGstPlayerCallback
{
public:

	virtual int OnDVDNavResult(void* pData, int iMessage) { return 0; };
	virtual void GetVideoResolution(unsigned int &width, unsigned int &height);
	
	CGstPlayer(IPlayerCallback& callback);
	virtual ~CGstPlayer();
	virtual bool Initialize(TiXmlElement* pConfig);
	virtual bool OpenFile(const CFileItem& file, const CPlayerOptions &options);
	virtual bool CloseFile(bool reopen = false);
	
	//virtual bool IsPlaying() const { return m_isPlaying; };
	virtual bool IsPlaying() const;
	virtual void Pause() override;
	virtual bool HasVideo() const;
	virtual bool HasAudio() const;
	virtual void ToggleOSD(); // empty
	virtual void SwitchToNextLanguage();
	virtual void ToggleSubtitles();
	virtual bool CanSeek();
	virtual void Seek(bool bPlus, bool bLargeStep, bool bChapterOverride);
	virtual void SeekPercentage(float iPercent);
	virtual float GetPercentage();
	virtual void SetVolume(float volume);
	virtual void SetMute(bool bOnOff);
	
	virtual bool SwitchChannel(const CPVRChannelPtr &channel);

	virtual bool HasMenu() const { return true; };
	virtual void SetDynamicRangeCompression(long drc) {}
	virtual void SetContrast(bool bPlus) {}
	virtual void SetBrightness(bool bPlus) {}
	virtual void SetHue(bool bPlus) {}
	virtual void SetSaturation(bool bPlus) {}
	virtual void SwitchToNextAudioLanguage();
	virtual bool CanRecord();
	virtual bool IsRecording();
	virtual bool Record(bool bOnOff) { return false; }
	virtual void SetAVDelay(float fValue = 0.0f);
	virtual float GetAVDelay();
	
	virtual void FrameMove();
	virtual void Render(bool clear, uint32_t alpha, bool gui);
	virtual void FlushRenderer() { CLog::Log(LOGNOTICE, "%s: ", __FUNCTION__ ); };
	
	virtual void SetRenderViewMode(int mode) { CLog::Log(LOGNOTICE, "%s: mode=%d", __FUNCTION__, mode); };
	virtual float GetRenderAspectRatio();
	virtual void TriggerUpdateResolution() { CLog::Log(LOGNOTICE, "%s: ", __FUNCTION__ ); };
	
	virtual bool IsRenderingVideo() { return true; };
	virtual bool IsRenderingGuiLayer() { return true; };
	virtual bool IsRenderingVideoLayer() { return true; };
	
	virtual void SetSubTitleDelay(float fValue = 0.0f);
	virtual float GetSubTitleDelay();

	virtual void SeekTime(int64_t iTime);
	virtual bool SeekTimeRelative(int64_t iTime);
	
	virtual int64_t GetTime();
	virtual int64_t GetTotalTime();
	virtual void SetSpeed(float iSpeed) override;
	virtual void ShowOSD(bool bOnoff);
	virtual void DoAudioWork() {};
	
	virtual std::string GetPlayerState();
	virtual bool SetPlayerState(const std::string& state);
	
	virtual int GetAudioStreamCount();
	virtual int GetVideoStreamCount();
	virtual int GetSubtitleCount();
	
	virtual int GetAudioStream();
	virtual int GetVideoStream();
	virtual int GetSubtitle();
	
	virtual bool SupportsTempo() { return m_canTempo; };
	
	void OnPlaybackStarted();

private:

	virtual void Process();
	virtual bool OnAction(const CAction &action);
	
	void CreatePlayers();
	void DestroyPlayers();
	
	bool ShowPVRChannelInfo();
	
	bool m_bAbortRequest;
	bool m_isPlaying;
	bool m_isRecording;
	
	int64_t m_playbackStartTime;
	int m_speed;
	int m_totalTime;
	int m_time;
	int m_playCountMinTime;
	
	CPlayerOptions m_PlayerOptions;
	std::unique_ptr<CProcessInfo> m_processInfo;
	
//	CDVDMessageQueue m_messenger;		 // thread messenger
 
	CGstPlayerAudio *m_VideoPlayerAudio;
	CGstPlayerVideo *m_VideoPlayerVideo;
	
	CEvent m_ready;
	
protected:
	bool OpenInputStream();
	
	friend class CSelectionStreams;
	
	CFileItem m_item;
	std::atomic_bool m_canTempo;
	CDVDInputStream* m_pInputStream;  // input stream for current playing file
};
