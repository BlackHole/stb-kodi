
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

#pragma once

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

	int OnDiscNavResult(void* pData, int iMessage) { return 0; };
	void GetVideoResolution(unsigned int &width, unsigned int &height);
	
	explicit CGstPlayer(IPlayerCallback& callback);
	~CGstPlayer() override;
	bool Initialize(TiXmlElement* pConfig);
	bool OpenFile(const CFileItem& file, const CPlayerOptions &options) override;
	bool CloseFile(bool reopen = false) override;
	
	//bool IsPlaying() const { return m_isPlaying; };
	bool IsPlaying() const;
	void Pause() override;
	bool HasVideo() const;
	bool HasAudio() const;
	void ToggleOSD(); // empty
	void SwitchToNextLanguage();
	void ToggleSubtitles();
	bool CanSeek();
	void Seek(bool bPlus, bool bLargeStep, bool bChapterOverride);
	void SeekPercentage(float iPercent);
	float GetPercentage();
	void SetVolume(float volume);
	void SetMute(bool bOnOff);

	bool HasMenu() const { return true; };
	void SetDynamicRangeCompression(long drc) {}
	void SetContrast(bool bPlus) {}
	void SetBrightness(bool bPlus) {}
	void SetHue(bool bPlus) {}
	void SetSaturation(bool bPlus) {}
	void SwitchToNextAudioLanguage();
	bool CanRecord();
	bool IsRecording();
	bool Record(bool bOnOff) { return false; }
	void SetAVDelay(float fValue = 0.0f);
	float GetAVDelay();
	
	void FrameMove();
	void Render(bool clear, uint32_t alpha, bool gui);
	void FlushRenderer() { CLog::Log(LOGNOTICE, "%s: ", __FUNCTION__ ); };
	
	void SetRenderViewMode(int mode) { CLog::Log(LOGNOTICE, "%s: mode=%d", __FUNCTION__, mode); };
	float GetRenderAspectRatio();
	void TriggerUpdateResolution() { CLog::Log(LOGNOTICE, "%s: ", __FUNCTION__ ); };
	
	bool IsRenderingVideo() { return true; };
	bool IsRenderingGuiLayer() { return true; };
	bool IsRenderingVideoLayer() { return true; };
	
	void SetSubTitleDelay(float fValue = 0.0f);
	float GetSubTitleDelay();

	void SeekTime(int64_t iTime);
	bool SeekTimeRelative(int64_t iTime);
	
	int64_t GetTime();
	int64_t GetTotalTime();
	void SetSpeed(float iSpeed) override;
	void ShowOSD(bool bOnoff);
	void DoAudioWork() {};
	
	std::string GetPlayerState();
	bool SetPlayerState(const std::string& state);
	
	int GetAudioStreamCount();
	int GetVideoStreamCount();
	int GetSubtitleCount();
	
	int GetAudioStream();
	int GetVideoStream();
	int GetSubtitle();
	
	bool SupportsTempo() { return m_canTempo; };
	
	void OnPlaybackStarted();

private:

	void Process();
	bool OnAction(const CAction &action);
	
	void CreatePlayers();
	void DestroyPlayers();
	
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
	std::shared_ptr<CDVDInputStream> m_pInputStream;  // input stream for current playing file
};
