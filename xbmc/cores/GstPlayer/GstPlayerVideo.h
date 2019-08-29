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
#include <vector>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "GstPlayerAudio.h"
#include "utils/log.h"
#include "cores/IPlayer.h"
#include "GstPlayer.h"
#include "cores/VideoPlayer/Process/ProcessInfo.h"
//#include "cores/VideoPlayer/DVDInputStreams/DVDInputStreamMemory.h"
#include "cores/VideoPlayer/DVDInputStreams/DVDInputStreamFile.h"
#include "cores/VideoPlayer/DVDInputStreams/DVDInputStream.h"

#include "DVDStreamInfo.h"
//#include "DVDMessageQueue.h"

#undef FALSE
#define FALSE 0

class CProcessInfo;
class IGstPlayerCallback;

typedef enum {
	GST_PLAY_FLAG_VIDEO		= (1 << 0),
	GST_PLAY_FLAG_AUDIO		= (1 << 1),
	GST_PLAY_FLAG_TEXT		= (1 << 2),
	GST_PLAY_FLAG_VIS		= (1 << 3),
	GST_PLAY_FLAG_SOFT_VOLUME	= (1 << 4),
	GST_PLAY_FLAG_NATIVE_AUDIO	= (1 << 5),
	GST_PLAY_FLAG_NATIVE_VIDEO	= (1 << 6),
	GST_PLAY_FLAG_DOWNLOAD		= (1 << 7),
	GST_PLAY_FLAG_BUFFERING		= (1 << 8),
	GST_PLAY_FLAG_DEINTERLACE	= (1 << 9)
} GstPlayFlags;

class CGstPlayerVideo
{
public:
	CGstPlayerVideo(IGstPlayerCallback *callback, CProcessInfo &processInfo);
	~CGstPlayerVideo();
	bool OpenStream(CDVDStreamInfo &hints, const CFileItem& file);
	void CloseStream();
	void Pause();
	void SwitchToNextLanguage();
	void ToggleSubtitles();
	bool CanSeek();
	void Seek(bool bPlus, bool bLargeStep, bool bChapterOverride);
	void SwitchToNextAudioLanguage();
	void SeekPercentage(float iPercent);
	void SetAVDelay(float fValue);
	float GetAVDelay();
	void SetSubTitleDelay(float fValue);
	float GetSubTitleDelay();
	void SeekTime(int64_t iTime);
	bool SeekTimeRelative(int64_t iTime);
	int64_t GetTime();
	int64_t GetTotalTime();
	std::string GetPlayerState();
	float GetRenderAspectRatio();
	
	int GetAudioStreamCount() { CLog::Log(LOGNOTICE, "%s: m_audio: %d", __FUNCTION__, m_audio); return (int)m_audio; };
	int GetVideoStreamCount() { CLog::Log(LOGNOTICE, "%s: m_video: %d", __FUNCTION__, m_video); return (int)m_video; };
	int GetSubtitleCount()    { CLog::Log(LOGNOTICE, "%s: m_text: %d", __FUNCTION__, m_text); return (int)m_text; };
	
	int GetAudioStream() { CLog::Log(LOGNOTICE, "%s: m_current_audio: %d", __FUNCTION__, m_current_audio); return (int)m_current_audio; };
	int GetVideoStream() { CLog::Log(LOGNOTICE, "%s: m_current_video: %d", __FUNCTION__, m_current_video); return (int)m_current_video; };
	int GetSubtitle()    { CLog::Log(LOGNOTICE, "%s: m_current_text: %d", __FUNCTION__, m_current_text); return (int)m_current_text; };
	
	struct sourceStream
	{
//		audiotype_t audiotype;
//		containertype_t containertype;
		gboolean is_audio;
		gboolean is_video;
		gboolean is_streaming;
		gboolean is_hls;
		sourceStream()
			: /*audiotype(atUnknown), containertype(ctNone), */is_audio(FALSE), is_video(FALSE), is_streaming(FALSE), is_hls(FALSE)
		{
		}
	};
	
	struct bufferInfo
	{
		gint bufferPercent;
		gint avgInRate;
		gint avgOutRate;
		gint64 bufferingLeft;
		bufferInfo()
			:bufferPercent(0), avgInRate(0), avgOutRate(0), bufferingLeft(-1)
		{
		}
	};
	
private:
	bool CreateFakePipeline();
	bool CreatePipeline();
	void DestroyPipeline();
	
	static void handleElementAdded(GstBin *bin, GstElement *element, gpointer user_data);
	static void NotifySource(GObject *object, GParamSpec *unused, gpointer user_data);
	static void DeepNotifySource(GObject *object, GObject *orig, GParamSpec *pspec, gpointer user_data);
	static void stopFeed(GstElement * playbin, gpointer user_data);
	static void startFeed(GstElement * playbin, guint size, gpointer user_data);
	static gboolean readData(gpointer user_data); 
	static gboolean seekData(GstElement * appsrc, guint64 position, gpointer user_data);
	static GstBusSyncReply gstBusSyncHandler(GstBus *bus, GstMessage *msg, gpointer data);
	void handleMessage(GstMessage *msg);
	void OnPipelineStart();
	
	int m_totalTime;
	int m_time;
	int m_playCountMinTime;
	int m_buffer_size;
	int m_ignore_buffering_messages;
	
	bufferInfo m_bufferInfo;
	sourceStream m_sourceinfo;
	
	std::string m_useragent;
	std::string m_extra_headers;
	std::string m_download_buffer_path;
	
	bool m_use_prefillbuffer;
	bool m_is_live;
	bool m_first_paused;
	bool m_paused;
	
	IGstPlayerCallback *m_callback;
	
	gchar *m_uri;
	GMainLoop *m_loop;
	GstElement *m_playbin, *m_appsrc;
	gint m_aspect, m_width, m_height, m_framerate, m_progressive, m_video, m_audio, m_text, m_current_video, m_current_audio, m_current_text;
	gulong m_notify_source_handler_id;
	guint m_notify_element_added_handler_id, m_notify_source_id;
	gsize m_length;
	
	guint64 m_offset;
	
protected:
	CFileItem m_item;
	CProcessInfo &m_processInfo;
	//CDVDInputStreamMemory *m_pInputStream;  // input stream for current playing file
	CDVDInputStreamFile *m_pInputStream;

};
