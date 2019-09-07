/*
 *			Copyright (C) 2005-2015 Team Kodi
 *			http://kodi.tv
 *
 *	This Program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This Program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with Kodi; see the file COPYING.	If not, see
 *	<http://www.gnu.org/licenses/>.
 *
 */

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/missing-plugins.h>

#include "GstPlayerVideo.h"
#include "system.h"
#include "URL.h"
#include "Util.h"
#include "ServiceBroker.h"
#include "filesystem/SpecialProtocol.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "messaging/ApplicationMessenger.h"

#define HTTP_TIMEOUT 10

#define CHUNK_SIZE 4096

CGstPlayerVideo::CGstPlayerVideo(IGstPlayerCallback *callback, CProcessInfo &processInfo): m_callback(callback)
	,m_processInfo(processInfo)
	,m_aspect(0)
	,m_width(0)
	,m_height(0)
	,m_offset(0)
	,m_length(0)
	,m_framerate(0)
	,m_progressive(0)
	,m_time(0)
	,m_video(0)
	,m_audio(0)
	,m_text(0)
	,m_totalTime(1)
	,m_uri(NULL)
	,m_loop(NULL)
	,m_playbin(NULL)
	,m_appsrc(NULL)
	,m_pInputStream(NULL)
	,m_extra_headers("")
	,m_download_buffer_path("")
	,m_notify_source_handler_id(0)
	,m_notify_source_id(0)
	,m_notify_element_added_handler_id(0)
	,m_ignore_buffering_messages(0)
	,m_use_prefillbuffer(false)
	,m_is_live(false)
	
{
	gst_init (NULL, FALSE);
	m_processInfo.SetVideoDecoderName(("Gstreamer Version: %s", (const char *)gst_version_string()), true);
	m_processInfo.SetAudioDecoderName(("Gstreamer Version: %s", (const char *)gst_version_string()));
	const std::shared_ptr<CAdvancedSettings> advancedSettings = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings();
	m_useragent = advancedSettings->m_userAgent.c_str();
	m_buffer_size = 5LL * 1024LL * 1024LL;
	m_sourceinfo.is_video = TRUE;
	m_paused = false;
	m_first_paused = false;
}

CGstPlayerVideo::~CGstPlayerVideo()
{
	CloseStream();
	
	if(m_pInputStream)
		SAFE_DELETE(m_pInputStream);
	
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
}

bool CGstPlayerVideo::OpenStream(CDVDStreamInfo &hints, const CFileItem &file)
{
	m_item = file;
	m_processInfo.ResetVideoCodecInfo();
	m_sourceinfo.is_streaming = FALSE;
	m_sourceinfo.is_hls = TRUE;
	
	if(m_pInputStream)
		SAFE_DELETE(m_pInputStream);
	
	std::string url(hints.filename);
	
	if (url.empty())
		return CreateFakePipeline();
	
	if(StringUtils::EndsWith(url, ".m3u8"))
		m_sourceinfo.is_hls = TRUE;
	
	if(StringUtils::StartsWith(url, "smb://") || StringUtils::StartsWith(url, "upnp://") || StringUtils::StartsWith(url, "nfs://"))
	{
		//m_sourceinfo.is_streaming = TRUE;

		m_pInputStream = new CDVDInputStreamFile(m_item);
		
		m_uri = g_strdup((const gchar *) "appsrc://");
	}
	
	else if(url.find("://") != std::string::npos)
	{
		m_sourceinfo.is_streaming = TRUE;
		size_t pos = url.find('#');
		
		if (pos != std::string::npos && (StringUtils::StartsWith(url, "http") || StringUtils::StartsWith(url, "rtsp")))
		{
			m_extra_headers = url.substr(pos + 1);
			pos = m_extra_headers.find("User-Agent=");
			if (pos != std::string::npos)
			{
				size_t hpos_start = pos + 11;
				size_t hpos_end = m_extra_headers.find('&', hpos_start);
				if (hpos_end != std::string::npos)
					m_useragent = m_extra_headers.substr(hpos_start, hpos_end - hpos_start);
				else
					m_useragent = m_extra_headers.substr(hpos_start);
			}
		}
		m_uri = g_strdup((const gchar *) url.c_str());
	}
	
	else if(StringUtils::StartsWith(url, "/"))
		m_uri = g_filename_to_uri((const gchar *) url.c_str(), NULL, NULL);
	
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: is_streaming: %s", __FUNCTION__, m_sourceinfo.is_streaming ? "true" : "false");
	
	return CreatePipeline();
}

void CGstPlayerVideo::CloseStream()
{
	DestroyPipeline();
}

void CGstPlayerVideo::DestroyPipeline()
{
	if (m_playbin != NULL)
	{
		GstStateChangeReturn ret;
		
		if(m_notify_source_handler_id)
		{
			g_signal_handler_disconnect(m_playbin, m_notify_source_handler_id);
			m_notify_source_handler_id = 0;
		}
		
		if(m_notify_element_added_handler_id)
		{
			g_signal_handler_disconnect(m_playbin, m_notify_element_added_handler_id);
			m_notify_element_added_handler_id = 0;
		}
		
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE (m_playbin));
		gst_bus_set_sync_handler(bus, NULL, NULL, NULL);
		gst_object_unref(bus);
		
		ret = gst_element_set_state(m_playbin, GST_STATE_NULL);
		if (ret != GST_STATE_CHANGE_SUCCESS)
			CLog::Log(LOGFATAL, "CGstPlayerVideo::%s: Failed to set pipeline to GST_STATE_NULL!", __FUNCTION__);
		else
			CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: set pipeline to GST_STATE_NULL!", __FUNCTION__);

		gst_object_unref(GST_OBJECT(m_playbin));
		m_appsrc = NULL;
		m_playbin = NULL;
	}
	
	if(m_pInputStream)
	{
		m_pInputStream->Close();
		SAFE_DELETE(m_pInputStream);
		printf("CGstPlayerVideo::%s: SAFE_DELETE m_pInputStream", __FUNCTION__);
	}
	
	if (m_uri != NULL)
	{
		g_free(m_uri);
		m_uri = NULL;
	}
	
	if (m_loop != NULL)
	{
		g_main_loop_quit(m_loop);
	}
	
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: clean", __FUNCTION__);
}

bool CGstPlayerVideo::CreateFakePipeline()
{
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: running pipeline...", __FUNCTION__);
	
	m_callback->OnPlaybackStarted();
	
	m_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(m_loop);
	
	g_main_loop_unref(m_loop);
	m_loop = NULL;
	
	return true;
}

bool CGstPlayerVideo::CreatePipeline()
{
	m_loop = g_main_loop_new(NULL, FALSE);
	m_playbin = gst_element_factory_make("playbin", "playbin");
	
	if(m_playbin)
	{
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: m_uri: '%s'", __FUNCTION__, m_uri);
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: running pipeline...", __FUNCTION__);

		int flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO;
		if(m_pInputStream)
		{
			if (m_pInputStream->Open())
			{
				m_length = (gsize) m_item.m_dwSize;
				//m_length = (gsize) abs(m_pInputStream->GetLength());
				m_offset = 0;
				m_pInputStream->Seek(0, SEEK_SET);
				m_notify_source_handler_id = g_signal_connect(m_playbin, "deep-notify::source", G_CALLBACK (DeepNotifySource), this);
				CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: m_length=%d m_pInputStream->GetLength()=%d\n", __FUNCTION__,(int)m_length, m_pInputStream->GetLength());
				printf("CGstPlayerVideo::%s: m_pInputStream->GetLength() %d\n", __FUNCTION__,(int)m_length);
			}
			else
			{
				if (m_loop != NULL) 
				{
					g_main_loop_unref(m_loop);
					m_loop = NULL;
				}
				printf("CGstPlayerVideo::%s: m_pInputStream->Open()  Can not open File!\n", __FUNCTION__);
				CLog::Log(LOGERROR, "CGstPlayerVideo::%s: Can not open File!",__FUNCTION__);
				return false;
			}
				
		}
		
		else if ( m_sourceinfo.is_streaming )
		{
			m_download_buffer_path = CSpecialProtocol::TranslatePath(CUtil::GetNextFilename("special://temp/filecache%03d.cache", 999));
			

			m_notify_source_handler_id = g_signal_connect(m_playbin, "notify::source", G_CALLBACK (NotifySource), this);
			
			if (m_download_buffer_path.empty())
			{
				CLog::Log(LOGERROR, "CGstPlayerVideo::%s: - Unable to generate a new filename for cache file", __FUNCTION__);
			}
			else
			{
				CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: m_download_buffer_path: %s", __FUNCTION__, m_download_buffer_path.c_str());
			
				if (access(m_download_buffer_path.c_str(), X_OK) >= 0)
				{
					CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: using cache file (buffer): %s", __FUNCTION__, m_download_buffer_path.c_str());
					/* It looks like /hdd points to a valid mount, so we can store a download buffer on it */
					m_use_prefillbuffer = true;
					flags |= GST_PLAY_FLAG_DOWNLOAD;
					m_notify_element_added_handler_id = g_signal_connect(m_playbin, "element-added", G_CALLBACK(handleElementAdded), this);
					/* limit file size */
					g_object_set(m_playbin, "ring-buffer-max-size", (guint64)(8LL * 1024LL * 1024LL), NULL);
				}
				else
				{
					CLog::Log(LOGERROR, "CGstPlayerVideo::%s: - Unable to access cache file %s", __FUNCTION__, m_download_buffer_path.c_str());
					m_download_buffer_path = "";
				}
			}
		
			flags |= GST_PLAY_FLAG_BUFFERING;
			/* increase the default 2 second / 2 MB buffer limitations to 10s / 10MB */
			g_object_set(m_playbin, "buffer-duration", (gint64)(5LL * GST_SECOND), NULL);
			g_object_set(m_playbin, "buffer-size", m_buffer_size, NULL);
		
			if(m_sourceinfo.is_hls)
				g_object_set(m_playbin, "connection-speed", (guint64)(4495000LL), NULL);
		}
		
		g_object_set(G_OBJECT(m_playbin), "uri", m_uri, NULL);
		g_object_set(G_OBJECT(m_playbin), "flags", flags, NULL);

		GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_playbin));
		gst_bus_set_sync_handler(bus, gstBusSyncHandler, this, NULL);
		gst_object_unref(bus);

		GstStateChangeReturn ready;
		ready = gst_element_set_state (m_playbin, GST_STATE_READY);

		switch(ready)
		{
		case GST_STATE_CHANGE_FAILURE:
			CLog::Log(LOGFATAL, "CGstPlayerVideo::%s: failed to start pipeline",__FUNCTION__);
			DestroyPipeline();
			return false;
			break;
		case GST_STATE_CHANGE_SUCCESS:
			gst_element_set_state (m_playbin, GST_STATE_PLAYING);
			printf("CGstPlayerVideo::%s:GST_STATE_PLAYING\n", __FUNCTION__);
			m_is_live = false;
			m_first_paused = true;
			break;
		case GST_STATE_CHANGE_NO_PREROLL:
			gst_element_set_state (m_playbin, GST_STATE_PLAYING);
			printf("CGstPlayerVideo::%s: GST_STATE_PLAYING\n", __FUNCTION__);
			m_is_live = true;
			break;
		default:
			return false;
			break;
		}
		
		g_main_loop_run(m_loop);
		
		g_main_loop_unref(m_loop);
		m_loop = NULL;
		
		DestroyPipeline();
	}
	else
	{
		if (m_loop != NULL) 
		{
			g_main_loop_unref(m_loop);
			m_loop = NULL;
		}
		CLog::Log(LOGFATAL, "CGstPlayerVideo::%s: Failed to create pipeline!",__FUNCTION__);
		return false;
	}
	
	return true;
}

void CGstPlayerVideo::Pause()
{
	if(m_playbin)
	{
		GstState currentState;
		
		gst_element_get_state(GST_ELEMENT(m_playbin), &currentState, 0, 0);
		
		if(currentState == GST_STATE_PAUSED)
		{
			gst_element_set_state(GST_ELEMENT(m_playbin), GST_STATE_PLAYING);
			CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: set play", __FUNCTION__ );
		}
		else
		{
			gst_element_set_state(GST_ELEMENT(m_playbin), GST_STATE_PAUSED);
			CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: set pause", __FUNCTION__ );
		}
	}
}

void CGstPlayerVideo::SwitchToNextLanguage()
{
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
}

void CGstPlayerVideo::ToggleSubtitles()
{
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
}

bool CGstPlayerVideo::CanSeek()
{
	if(m_pInputStream)
		return false;
	//CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: true",	__FUNCTION__ );
	return true;
}

void CGstPlayerVideo::Seek(bool bPlus, bool bLargeStep, bool bChapterOverride)
{
	if(m_playbin)
	{
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: bPlus=%s bLargeStep=%s bChapterOverride=%s", __FUNCTION__, bPlus == false ? "off":"on" , bLargeStep == false ? "off":"on" , bChapterOverride == false ? "off":"on" ); 
	}
}

void CGstPlayerVideo::SwitchToNextAudioLanguage()
{
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
}

void CGstPlayerVideo::SeekPercentage(float iPercent)
{
	CLog::Log(LOGERROR, "CGstPlayerVideo::%s: iPercent=%f", __FUNCTION__, iPercent);
	
	if(m_playbin)
	{
		
		gint64 position;
	
		if (!gst_element_query_position (GST_ELEMENT(m_playbin), GST_FORMAT_TIME, &position))
		{
			CLog::Log(LOGERROR, "CGstPlayerVideo::%s: Position query failed...", __FUNCTION__);
			
			return;
		}
		/* no position, seek to 1 second, this could EOS */
		if (position == -1)
		{
			CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: position=-1", __FUNCTION__);
			
			return;
		}


		int val = (int) GetTotalTime() * iPercent;
		
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: iPercent=%f seek to %d milliseconds", __FUNCTION__, iPercent, val);

		if(val<0)
			return;

		GstClockTime nanoSec = (GstClockTime) (val * GST_MSECOND);
		if (!gst_element_seek(m_playbin, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, nanoSec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
			CLog::Log(LOGERROR, "CGstPlayerVideo::%s: seek", __FUNCTION__ );
		//else:
		//	m_callback.OnPlayBackSeek((int)abstime, iTime);
	}
}

void CGstPlayerVideo::SetAVDelay(float fValue)
{
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
}

float CGstPlayerVideo::GetAVDelay()
{
	//CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
	return 0.0f;
}

void CGstPlayerVideo::SetSubTitleDelay(float fValue)
{
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
}

float CGstPlayerVideo::GetSubTitleDelay()
{
	//CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__ );
	return 0.0f;
}

void CGstPlayerVideo::SeekTime(int64_t iTime)
{
	int seekOffset = (int)(iTime - GetTime());
	
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: seekOffset=%d", __FUNCTION__, seekOffset);
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: iTime=%d", __FUNCTION__, iTime);
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: GetTime()=%d", __FUNCTION__, GetTime());
	
	printf("CGstPlayerVideo::%s: seekOffset=%d", __FUNCTION__, seekOffset);
	printf("CGstPlayerVideo::%s: iTime=%d", __FUNCTION__, iTime);
	printf("CGstPlayerVideo::%s: GetTime()=%d", __FUNCTION__, GetTime());
	
	if (seekOffset >= 0)
	{
		GstClockTime nanoSec = (GstClockTime) ((int)seekOffset * GST_MSECOND);
		if (!gst_element_seek(m_playbin, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, nanoSec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
			CLog::Log(LOGERROR, "CGstPlayerVideo::%s: seek", __FUNCTION__ );
	}
}

bool CGstPlayerVideo::SeekTimeRelative(int64_t iTime)
{
	int64_t abstime = GetTime() + iTime;
	
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: abstime=%d", __FUNCTION__, abstime);
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: iTime=%d", __FUNCTION__, iTime);
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: GetTime()=%d", __FUNCTION__, GetTime());
	printf("CGstPlayerVideo::%s: abstime=%d", __FUNCTION__, abstime);
	printf("CGstPlayerVideo::%s: iTime=%d", __FUNCTION__, iTime);
	printf("CGstPlayerVideo::%s: GetTime()=%d", __FUNCTION__, GetTime());
	
	if (abstime >= 0)
	{
		GstClockTime nanoSec = (GstClockTime) ((int)abstime * GST_MSECOND);
		if (!gst_element_seek(m_playbin, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, nanoSec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
			CLog::Log(LOGERROR, "CGstPlayerVideo::%s: seek", __FUNCTION__ );

		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: abstime=%d", __FUNCTION__, abstime);
	}
	
	return true;
}

int64_t CGstPlayerVideo::GetTime() // in milliseconds
{
	gint64 position;
	
	if (!gst_element_query_position (GST_ELEMENT(m_playbin), GST_FORMAT_TIME, &position))
	{
		m_time = (int64_t) 1000;
		CLog::Log(LOGERROR, "CGstPlayerVideo::%s: Position query failed...", __FUNCTION__);
	}
	/* no position, seek to 1 second, this could EOS */
	if (position == -1)
	{
		m_time = (int64_t) 1000;
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: milliseconds=1000 position=-1", __FUNCTION__);
	}
	else
	{
		m_time = (int64_t) GST_TIME_AS_MSECONDS(position);
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: milliseconds=%d", __FUNCTION__, (int) m_time);
	}
	
	if(m_pInputStream)
	{
		m_time = (int64_t) (((GetTotalTime() * 1000) / m_length) * m_offset) / 1000;
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: (appsrc) m_offset=%d m_length=%d m_pInputStream->GetLength()=%d", __FUNCTION__, (int) m_offset, (int) m_length, m_pInputStream->GetLength());
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: (appsrc) milliseconds=%d", __FUNCTION__, (int) m_time);
		return m_time;
	}
	
	return m_time;

}

int64_t CGstPlayerVideo::GetTotalTime() // in milliseconds
{
	gint64 duration = 0;
	
	if (!gst_element_query_duration(GST_ELEMENT(m_playbin), GST_FORMAT_TIME, &duration))
	{
		CLog::Log(LOGERROR, "CGstPlayerVideo::%s: Time duration query failed", __FUNCTION__);
		
		m_totalTime = 1;
	}
	else
	{
		/* no duration, seek to 1 second, this could EOS */
		if (duration != -1)
			m_totalTime = GST_TIME_AS_MSECONDS(duration)/1000;
		else
			m_totalTime = 1;
		
	}

	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: %d", __FUNCTION__, m_totalTime * 1000);
	
	return (int64_t) m_totalTime * 1000;
	
}

std::string CGstPlayerVideo::GetPlayerState()
{
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s", __FUNCTION__);
	
	return "";
}

void CGstPlayerVideo::OnPipelineStart()
{
	gint i;
	GstTagList *tags;
	gchar *str;
	guint rate;

	g_object_get (m_playbin, "n-video", &m_video, NULL);
	g_object_get (m_playbin, "n-audio", &m_audio, NULL);
	g_object_get (m_playbin, "n-text", &m_text, NULL);
	
	//m_processInfo.SetAudioChannels(m_audio);
	//m_processInfo.SetAudioSampleRate(GST_TAG_NOMINAL_BITRATE)
	
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: %d video stream(s), %d audio stream(s), %d text stream(s)", 
		 __FUNCTION__,
		(int)m_video,
		(int)m_audio,
		(int)m_text);
	
	for (i = 0; i < m_video; i++) {
		tags = NULL;
		/* Retrieve the stream's video tags */
		g_signal_emit_by_name (m_playbin, "get-video-tags", i, &tags);
		if (tags) {
			gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str);
			CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: video stream: %d codec: %s", 
				__FUNCTION__,
				(int)i,
				(char *) str ? str : "unknown");
			g_free (str);
			gst_tag_list_free (tags);
		}
	}
	
	for (i = 0; i < m_audio; i++) {
		tags = NULL;
		/* Retrieve the stream's audio tags */
		g_signal_emit_by_name (m_playbin, "get-audio-tags", i, &tags);
		if (tags) {
			CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: audio stream: %d", 
				__FUNCTION__,
				(int)i);
			if (gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str)) {
				CLog::Log(LOGNOTICE, "	codec: %s", (char *)str);
				g_free (str);
			}
			if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
				CLog::Log(LOGNOTICE, "	language: %s", (char *)str);
				g_free (str);
			}
			if (gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate)) {
				CLog::Log(LOGNOTICE, "	bitrate: %d", (int)rate);
			}
			gst_tag_list_free (tags);
		}
	}
	
	for (i = 0; i < m_text; i++) {
		tags = NULL;
		/* Retrieve the stream's subtitle tags */
		g_signal_emit_by_name (m_playbin, "get-text-tags", i, &tags);
		if (tags) {
			CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: subtitle stream: %d", 
				__FUNCTION__,
				(int)i);

			if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
				CLog::Log(LOGNOTICE, "	language: %s", (char *)str);
				g_free (str);
			}
			gst_tag_list_free (tags);
		}
	}
	
	g_object_get (m_playbin, "current-video", &m_current_video, NULL);
	g_object_get (m_playbin, "current-audio", &m_current_audio, NULL);
	g_object_get (m_playbin, "current-text", &m_current_text, NULL);
	
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: current video stream: %d, current audio stream: %d, current text stream: %d", 
		 __FUNCTION__,
		(int)m_current_video,
		(int)m_current_audio,
		(int)m_current_text);
}

float CGstPlayerVideo::GetRenderAspectRatio()
{
	CLog::Log(LOGNOTICE, "%s: m_aspect: %d.0f", __FUNCTION__, (int)m_aspect); 
	return (float) (1.0f * (int)m_aspect);
	
}

void CGstPlayerVideo::NotifySource(GObject *object, GParamSpec *unused, gpointer user_data)
{
	GstElement *source = NULL;
	CGstPlayerVideo *_this = (CGstPlayerVideo*)user_data;
	g_object_get(object, "source", &source, NULL);
	if (source)
	{
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "timeout") != 0)
		{
			GstElementFactory *factory = gst_element_get_factory(source);
			if (factory)
			{
				const gchar *sourcename = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
				if (!strcmp(sourcename, "souphttpsrc"))
				{
					g_object_set(G_OBJECT(source), "timeout", HTTP_TIMEOUT, NULL);
				}
			}
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "ssl-strict") != 0)
		{
			g_object_set(G_OBJECT(source), "ssl-strict", FALSE, NULL);
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "user-agent") != 0 && !_this->m_useragent.empty())
		{
			g_object_set(G_OBJECT(source), "user-agent", _this->m_useragent.c_str(), NULL);
		}
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "extra-headers") != 0 && !_this->m_extra_headers.empty())
		{
			GstStructure *extras = gst_structure_new_empty("extras");
			size_t pos = 0;
			while (pos != std::string::npos)
			{
				std::string name, value;
				size_t start = pos;
				size_t len = std::string::npos;
				pos = _this->m_extra_headers.find('=', pos);
				if (pos != std::string::npos)
				{
					len = pos - start;
					pos++;
					name = _this->m_extra_headers.substr(start, len);
					start = pos;
					len = std::string::npos;
					pos = _this->m_extra_headers.find('&', pos);
					if (pos != std::string::npos)
					{
						len = pos - start;
						pos++;
					}
					value = _this->m_extra_headers.substr(start, len);
				}
				if (!name.empty() && !value.empty())
				{
					GValue header;
					CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: setting extra-header '%s:%s'",
						  __FUNCTION__, 
						name.c_str(), 
						value.c_str());
					memset(&header, 0, sizeof(GValue));
					g_value_init(&header, G_TYPE_STRING);
					g_value_set_string(&header, value.c_str());
					gst_structure_set_value(extras, name.c_str(), &header);
				}
				else
				{
					CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: Invalid header format %s",
								__FUNCTION__,
								_this->m_extra_headers.c_str());
					break;
				}
			}
			if (gst_structure_n_fields(extras) > 0)
			{
				g_object_set(G_OBJECT(source), "extra-headers", extras, NULL);
			}
			gst_structure_free(extras);
		}
		gst_object_unref(source);
	}
}

void CGstPlayerVideo::DeepNotifySource(GObject *object, GObject *orig, GParamSpec *pspec, gpointer user_data)
{
	CGstPlayerVideo *_this = (CGstPlayerVideo*)user_data;
	
	g_object_get (orig, pspec->name, &_this->m_appsrc, NULL);
	printf("CGstPlayerVideo::%s: appsrc %p\n", __FUNCTION__, _this->m_appsrc);

	g_object_set (_this->m_appsrc, "size", (gint64) _this->m_length, NULL);

	gst_util_set_object_arg (G_OBJECT (_this->m_appsrc), "do-timestamp", "true");
	//gst_util_set_object_arg (G_OBJECT (_this->m_appsrc), "format", "time");
	gst_util_set_object_arg (G_OBJECT (_this->m_appsrc), "stream-type", "seekable");
	
	g_signal_connect (_this->m_appsrc, "need-data", G_CALLBACK (startFeed), user_data);
	g_signal_connect (_this->m_appsrc, "seek-data", G_CALLBACK (seekData), user_data);
	g_signal_connect (_this->m_appsrc, "enough-data", G_CALLBACK (stopFeed), user_data);
}

void CGstPlayerVideo::startFeed(GstElement * playbin, guint size, gpointer user_data)
{
	printf("CGstPlayerVideo::%s:", __FUNCTION__);
	CGstPlayerVideo *_this = (CGstPlayerVideo*)user_data;
	
	if (_this->m_notify_source_id == 0)
	{
		_this->m_notify_source_id = g_idle_add ((GSourceFunc) readData, user_data);
	}
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
void CGstPlayerVideo::stopFeed(GstElement * playbin, gpointer user_data)
{
	printf("CGstPlayerVideo::%s:", __FUNCTION__);
	CGstPlayerVideo *_this = (CGstPlayerVideo*)user_data;
	
	if (_this->m_notify_source_id != 0)
	{
		g_source_remove (_this->m_notify_source_id);
		_this->m_notify_source_id = 0;
	}
}

gboolean CGstPlayerVideo::seekData(GstElement * appsrc, guint64 position, gpointer user_data)
{
	CGstPlayerVideo *_this = (CGstPlayerVideo*)user_data;
	
	printf("CGstPlayerVideo::seekData: offset=%" G_GUINT64_FORMAT " m_offset=%d m_length=%d", position, _this->m_offset, _this->m_length);
	CLog::Log(LOGNOTICE, "CGstPlayerVideo::seekData: offset=%" G_GUINT64_FORMAT " m_offset=%d m_length=%d", position, _this->m_offset, _this->m_length);
	_this->m_offset = position;

	return TRUE;
}

gboolean CGstPlayerVideo::readData(gpointer user_data)
{
	printf("CGstPlayerVideo::%s:", __FUNCTION__);
	CGstPlayerVideo *_this = (CGstPlayerVideo*)user_data;
	
	GstMapInfo map;
	GstBuffer *buffer = NULL;
	guint len;
	GstFlowReturn ret;

	if (_this->m_offset >= _this->m_length) {
		/* we are EOS, send end-of-stream and remove the source */
		CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s: end-of-stream", __FUNCTION__);
		g_signal_emit_by_name (_this->m_appsrc, "end-of-stream", &ret);
		return FALSE;
	}

	len = CHUNK_SIZE;
	if (_this->m_offset + len > _this->m_length)
		len = _this->m_length - _this->m_offset;
	
	/* read the next chunk */
	buffer = gst_buffer_new_allocate(NULL, len, NULL);
			
	gst_buffer_map (buffer, &map, GST_MAP_WRITE);
	
	_this->m_pInputStream->Seek(_this->m_offset, SEEK_SET);
	_this->m_pInputStream->Read(map.data, len);

	printf("CGstPlayerVideo::%s:feed buffer %p, offset %" G_GUINT64_FORMAT "-%u", __FUNCTION__, buffer, _this->m_offset, len);
	g_signal_emit_by_name (_this->m_appsrc, "push-buffer", buffer, &ret);
	gst_buffer_unmap(buffer, &map);
	if (ret != GST_FLOW_OK) {
		/* some error, stop sending data */
		return FALSE;
	}

	_this->m_offset += len;

	return TRUE;
}

void CGstPlayerVideo::handleElementAdded(GstBin *bin, GstElement *element, gpointer user_data)
{
	CGstPlayerVideo *_this = (CGstPlayerVideo*)user_data;
	if (_this)
	{
		gchar *elementname = gst_element_get_name(element);

		if (g_str_has_prefix(elementname, "queue2"))
		{
			if (_this->m_download_buffer_path != "")
			{
				g_object_set(G_OBJECT(element), "temp-template", _this->m_download_buffer_path.c_str(), NULL);
			}
			else
			{
				g_object_set(G_OBJECT(element), "temp-template", NULL, NULL);
			}
		}
		else if (g_str_has_prefix(elementname, "uridecodebin")
			|| g_str_has_prefix(elementname, "decodebin"))
		{
			/*
			 * Listen for queue2 element added to uridecodebin/decodebin2 as well.
			 * Ignore other bins since they may have unrelated queues
			 */
			g_signal_connect(element, "element-added", G_CALLBACK(handleElementAdded), user_data);
		}
		g_free(elementname);
	}
}

GstBusSyncReply CGstPlayerVideo::gstBusSyncHandler(GstBus *bus, GstMessage *msg, gpointer data)
{
	CGstPlayerVideo *_this = (CGstPlayerVideo *)data;
	
	if (_this) _this->handleMessage(msg);
	
	return GST_BUS_DROP;
}

void CGstPlayerVideo::handleMessage(GstMessage *msg)
{
	if (!msg)
		return;
#if 0
	gchar *sourceName;
	GstObject *source;
	source = GST_MESSAGE_SRC(msg);
	if (!GST_IS_OBJECT(source))
		return;
	
	sourceName = gst_object_get_name(source);
#endif
	GstState state, pending, old_state, new_state;
	GstStateChangeReturn ret;
	GstStateChange transition;
	
	switch (GST_MESSAGE_TYPE(msg))
	{
		case GST_MESSAGE_EOS:
		{
			g_print("CGstPlayerVideo::%s: End of stream\n", __FUNCTION__);
			g_main_loop_quit(m_loop);
			break;
		}

		case GST_MESSAGE_BUFFERING:
		{
			if (m_sourceinfo.is_streaming)
			{
				//GstBufferingMode mode;
				gst_message_parse_buffering(msg, &(m_bufferInfo.bufferPercent));
				// eDebug("[eServiceMP3] Buffering %u percent done", m_bufferInfo.bufferPercent);
				//gst_message_parse_buffering_stats(msg, &mode, &(m_bufferInfo.avgInRate), &(m_bufferInfo.avgOutRate), &(m_bufferInfo.bufferingLeft));
				//m_event((iPlayableService*)this, evBuffering);
				/*
				 * we don't react to buffer level messages, unless we are configured to use a prefill buffer
				 * (even if we are not configured to, we still use the buffer, but we rely on it to remain at the
				 * healthy level at all times, without ever having to pause the stream)
				 *
				 * Also, it does not make sense to pause the stream if it is a live stream
				 * (in which case the sink will not produce data while paused, so we won't
				 * recover from an empty buffer)
				 */
				if (m_use_prefillbuffer && !m_is_live && !m_sourceinfo.is_hls && --m_ignore_buffering_messages <= 0)
				{
					if (m_bufferInfo.bufferPercent == 100)
					{
						GstState state, pending;
						/* avoid setting to play while still in async state change mode */
						gst_element_get_state(m_playbin, &state, &pending, 5 * GST_SECOND);
						if (state != GST_STATE_PLAYING && !m_first_paused)
						{
							g_print("CGstPlayerVideo::%s: *** PREFILL BUFFER action start playing *** pending state was %s", 
								__FUNCTION__ , 
								pending == GST_STATE_VOID_PENDING ? "NO_PENDING" : "A_PENDING_STATE" );
							gst_element_set_state (m_playbin, GST_STATE_PLAYING);
						}
						/*
						 * when we start the pipeline, the contents of the buffer will immediately drain
						 * into the (hardware buffers of the) sinks, so we will receive low buffer level
						 * messages right away.
						 * Ignore the first few buffering messages, giving the buffer the chance to recover
						 * a bit, before we start handling empty buffer states again.
						 */
						m_ignore_buffering_messages = 10;
					}
					else if (m_bufferInfo.bufferPercent == 0 && !m_first_paused)
					{
						g_print("CGstPlayerVideo::%s: *** PREFILLBUFFER action start pause ***", 
							__FUNCTION__);
						gst_element_set_state (m_playbin, GST_STATE_PAUSED);
						m_ignore_buffering_messages = 0;
					}
					else
					{
						m_ignore_buffering_messages = 0;
					}
				}
				
			}
			break;
		}
#if 1
		case GST_MESSAGE_STATE_CHANGED:
		{
			g_print("CGstPlayerVideo::%s: Message STATE_CHANGED\n", __FUNCTION__);
			GstState old_state, new_state;
			gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
			
			if (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (m_playbin) && 
				new_state == GST_STATE_PLAYING)
			{
				OnPipelineStart();
				m_callback->OnPlaybackStarted();
			}

			g_print("CGstPlayerVideo::%s: Element %s changed state from %s to %s.\n", 
				__FUNCTION__,
				GST_OBJECT_NAME(msg->src),
				gst_element_state_get_name (old_state),
				gst_element_state_get_name (new_state));
			break;
		}
#else
		case GST_MESSAGE_STATE_CHANGED:
		{
			if(GST_MESSAGE_SRC(msg) != GST_OBJECT(m_playbin))
				break;
			
			gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
			
			if(old_state == new_state)
				break;
			
			g_print("CGstPlayerVideo::%s:  ****STATE TRANSITION %s -> %s ****\n",
				__FUNCTION__,
				gst_element_state_get_name(old_state), 
				gst_element_state_get_name(new_state));
			
			transition = (GstStateChange)GST_STATE_TRANSITION(old_state, new_state);
			
			
			g_print("CGstPlayerVideo::%s:  ****TRANSITION %d ****\n",
				__FUNCTION__,
				transition);
			
			switch(transition)
			{
				case GST_STATE_CHANGE_NULL_TO_READY:
				{
					g_print("CGstPlayerVideo::%s:  STATE IS: GST_STATE_CHANGE_NULL_TO_READY ****\n",
						__FUNCTION__);

				}	break;
				
				case GST_STATE_CHANGE_READY_TO_PAUSED:
				{
					g_print("CGstPlayerVideo::%s:  STATE IS: GST_STATE_CHANGE_READY_TO_PAUSED ****\n",
						__FUNCTION__);
				}	break;
				
				case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
				{
					g_print("CGstPlayerVideo::%s:  STATE IS: GST_STATE_CHANGE_PAUSED_TO_PLAYING ****\n",
						__FUNCTION__);
					m_paused = false;
					if (!m_first_paused)
					{
						g_print("CGstPlayerVideo::%s: GST_STATE_CHANGE_PAUSED_TO_PLAYING !m_first_paused\n",
						__FUNCTION__);
					}
					else
					{
						g_print("CGstPlayerVideo::%s: GST_STATE_CHANGE_PAUSED_TO_PLAYING m_first_paused\n",
						__FUNCTION__);
						OnPipelineStart();
						m_callback->OnPlaybackStarted();
					}
					m_first_paused = false;
				}	break;
				
				case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
				{
					m_paused = true;
					g_print("CGstPlayerVideo::%s: GST_STATE_CHANGE_PLAYING_TO_PAUSED\n",
						__FUNCTION__);
				}	break;
				
				case GST_STATE_CHANGE_PAUSED_TO_READY:
				{
					g_print("CGstPlayerVideo::%s: GST_STATE_CHANGE_PAUSED_TO_READY\n",
						__FUNCTION__);
				}	break;
				
				case GST_STATE_CHANGE_READY_TO_NULL:
				{
					g_print("CGstPlayerVideo::%s: GST_STATE_CHANGE_READY_TO_NULL\n",
						__FUNCTION__);
				}	break;
			}
			break;
		}
#endif

		case GST_MESSAGE_ELEMENT:
		{
			const GstStructure *msgstruct = gst_message_get_structure(msg);
			if (msgstruct)
			{
				if ( gst_is_missing_plugin_message(msg) )
				{
					GstCaps *caps = NULL;
					gst_structure_get (msgstruct, "detail", GST_TYPE_CAPS, &caps, NULL);
					if (caps)
					{
						std::string codec = (const char*) gst_caps_to_string(caps);
						gchar *description = gst_missing_plugin_message_get_description(msg);
						if ( description )
						{
							CLog::Log(LOGERROR, "CGstPlayerVideo::%s: missing codec: %s",
								__FUNCTION__,
								codec.c_str());
							CLog::Log(LOGERROR, "CGstPlayerVideo::%s: GStreamer plugin: %s not available!",
								__FUNCTION__,
								(const char*)description);
							g_free(description);
						}
						gst_caps_unref(caps);
					}
				}
				else
				{
					const gchar *eventname = gst_structure_get_name(msgstruct);
					if ( eventname )
					{
						if (!strcmp(eventname, "eventSizeChanged") || !strcmp(eventname, "eventSizeAvail"))
						{
							gst_structure_get_int (msgstruct, "aspect_ratio", &m_aspect);
							gst_structure_get_int (msgstruct, "width", &m_width);
							gst_structure_get_int (msgstruct, "height", &m_height);
							if (strstr(eventname, "Changed"))
							{
								m_processInfo.SetVideoDimensions((int)m_width, (int)m_height);
							}
						}
						else if (!strcmp(eventname, "eventFrameRateChanged") || !strcmp(eventname, "eventFrameRateAvail"))
						{
							gst_structure_get_int (msgstruct, "frame_rate", &m_framerate);
							if (strstr(eventname, "Changed"))
								m_processInfo.SetVideoFps((float)m_framerate * 1.0f);
						}
						else if (!strcmp(eventname, "eventProgressiveChanged") || !strcmp(eventname, "eventProgressiveAvail"))
						{
							gst_structure_get_int (msgstruct, "progressive", &m_progressive);
							//if (strstr(eventname, "Changed"))
								//m_processInfo.SetVideoDeintMethod("progressive: %d", (int)m_progressive);
						}
						else if (!strcmp(eventname, "redirect"))
						{
							const char *uri = gst_structure_get_string(msgstruct, "new-location");
							CLog::Log(LOGNOTICE, "CGstPlayerVideo::%s redirect to %s",
								__FUNCTION__,
								uri);
							gst_element_set_state (m_playbin, GST_STATE_NULL);
							g_object_set(m_playbin, "uri", uri, NULL);
							gst_element_set_state (m_playbin, GST_STATE_PLAYING);
						}
					}
				}
			}
			break;
		}

		case GST_MESSAGE_ERROR:
		{
			GError *err;
			gst_message_parse_error (msg, &err, NULL);
			if(err)
			{
				g_printerr ("CGstPlayerVideo::%s: Error - %s\n", __FUNCTION__, err->message);
				g_error_free (err);
				g_main_loop_quit(m_loop);
			}
			break;
		}
		
		case GST_MESSAGE_TAG:
		{
			GstTagList *tags;
			gst_message_parse_tag(msg, &tags);
			
			if(tags)
			{
				gchar *value;
				if(gst_tag_list_get_string(tags, GST_TAG_TITLE, &value))
				{
					g_print("CGstPlayerVideo::%s: Info - %s\n", __FUNCTION__, value);
					g_free(value);
				}
				gst_tag_list_free(tags);
			}
			break;
		}
			
		default:
			g_print("CGstPlayerVideo::%s: %" GST_PTR_FORMAT"\n", __FUNCTION__, msg);
			break;
	}
	//g_free (sourceName);
}
