/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "system.h"
#include "threads/SystemClock.h"
#include "GSTPlayer.h"
#include "GUIInfoManager.h"
#include "Application.h"
#include "messaging/ApplicationMessenger.h"
#include "FileItem.h"
#include "settings/AdvancedSettings.h"
//#include "settings/GUISettings.h"
#include "settings/Settings.h"
#include "music/tags/MusicInfoTag.h"
#include "utils/TimeUtils.h"
#include "utils/log.h"
#include "utils/MathUtils.h"
#include "filesystem/SpecialProtocol.h"

#include "ServiceBroker.h"
#include "guilib/GUIComponent.h"
#include "DVDCodecs/Video/DVDVideoCodec.h"

#ifdef HAS_VIDEO_PLAYBACK
#include "cores/VideoRenderers/RenderManager.h"
#endif

GSTPlayer::GSTPlayer(IPlayerCallback& callback, CRenderManager& renderManager) :
  IPlayer            (callback),
  CThread            ("GSTPlayer"),
  m_renderManager(renderManager)
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  int argc = 0;
  char**argv = NULL;

  m_bIsPlaying = false;
  m_bPaused = false;
  m_bQueueFailed = false;
  m_iSpeed = 1;

  m_timeOffset = 0;

  m_currentFile = new CFileItem;
  m_nextFile = new CFileItem;

  gst_init(&argc, &argv);
  m_pGstPlaybin = gst_element_factory_make("playbin2", "playbin");

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE (m_pGstPlaybin));
#if GST_VERSION_MAJOR < 1
  gst_bus_set_sync_handler(bus, gstBusSyncHandler, this);
#else
  gst_bus_set_sync_handler(bus, gstBusSyncHandler, this, NULL);
#endif
  gst_object_unref(bus);
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

GSTPlayer::~GSTPlayer()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);

  CloseFile();

  if (m_pGstPlaybin)
  {
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE (m_pGstPlaybin));
#if GST_VERSION_MAJOR < 1
    gst_bus_set_sync_handler(bus, NULL, NULL);
#else
    gst_bus_set_sync_handler(bus, NULL, NULL, NULL);
#endif
    gst_object_unref(bus);
  }

  gst_element_set_state(m_pGstPlaybin, GST_STATE_NULL);

/*
  if (audioSink)
  {
    gst_object_unref(GST_OBJECT(audioSink));
    audioSink = NULL;
  }

  if (videoSink)
  {
    gst_object_unref(GST_OBJECT(videoSink));
    videoSink = NULL;
  }
*/

  if (m_pGstPlaybin)
    gst_object_unref (GST_OBJECT (m_pGstPlaybin));
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

void GSTPlayer::OnExit()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);

}

bool GSTPlayer::OpenFile(const CFileItem& file, const CPlayerOptions &options)
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  gchar *uri;
  int flags;

  m_iSpeed = 1;
  m_bPaused = false;
  m_bStopPlaying = false;

/*  int audioMode = g_guiSettings.GetInt("audiooutput.mode");
  bool passthrough = AUDIO_IS_BITSTREAM(audioMode);
  
  printf("%s:%s[%d] bitstream=%d mode=%d\n", __FILE__, __func__, __LINE__, passthrough, audioMode);
  
  FILE* fpAc3 = fopen("/proc/stb/audio/ac3", "w");
  FILE* fpHdmiSource = fopen("/proc/stb/hdmi/audio_source", "w");
  if (passthrough)
  {
    fprintf(fpAc3, "%s", "passthrough");
    fprintf(fpHdmiSource, "%s", "spdif");
  }
  else
  {
    fprintf(fpAc3, "%s", "downmix");
    fprintf(fpHdmiSource, "%s", "pcm");
  }
  fclose(fpAc3);
  fclose(fpHdmiSource);*/

  CLog::Log(LOGINFO, "GSTPlayer: Playing %s", file.GetPath().c_str());

  m_timeOffset = (int64_t)(options.starttime * 1000);

  uri = g_filename_to_uri(file.GetPath().c_str(), NULL, NULL);
  if (uri == NULL)
    uri = strdup(file.GetPath().c_str());

  if (!g_ascii_strncasecmp(uri, "nfs://", 6)) {
    gchar* url = uri + 6;
    gchar* new_uri;
    gchar** tokens = g_strsplit(url, "/", 3);
    gchar* cmd = g_strdup_printf("mount -tnfs  -onolock %s:/%s /tmp/nfs", tokens[0], tokens[1]);
    
    new_uri = g_strdup_printf("file:///tmp/nfs/%s", tokens[2]);
    g_free(uri);
    uri = new_uri;
    
    printf ("Playing: %s\n", uri);
    
    printf ("mkdir -p /tmp/nfs\n");
    system("mkdir -p /tmp/nfs");
    printf ("umount /tmp/nfs\n");
    system("umount /tmp/nfs");
    printf ("%s\n", cmd);
    system(cmd);
    g_free(cmd);
  }

  g_object_set (G_OBJECT (m_pGstPlaybin), "uri", uri, NULL);
  g_free(uri);

  // ( GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO | GST_PLAY_FLAG_TEXT );
  flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO; 
  g_object_set (G_OBJECT (m_pGstPlaybin), "flags", flags, NULL);

  *m_currentFile = file;

  if (!IsRunning())
    Create();

  m_startEvent.Set();
  
  m_bIsPlaying = true;
  m_bQueueFailed = false;
  m_iCurrentAudioStream = -1;



    m_iVideoWidth = 1280;
    m_iVideoHeight = 720;
    m_iVideoFramerate = 50000;
    
    m_evVideoSizeChanged = 0;
    m_evVideoFramerateChanged = 0;
    m_evVideoProgressiveChanged = 0;

  gst_element_set_state (m_pGstPlaybin, GST_STATE_PLAYING);

#if defined(HAS_VIDEO_PLAYBACK)
     m_renderManager.PreInit();


     VideoPicture picture = {};
     picture.iWidth = m_iVideoWidth;
     picture.iHeight = m_iVideoHeight;
     picture.iDisplayWidth = 1280;
     picture.iDisplayHeight = 720;

    
     m_renderManager.Configure(picture, m_iVideoFramerate/1000.0, 0);
#endif

  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
  return true;
}

//Finished
bool GSTPlayer::QueueNextFile(const CFileItem &file)
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  if (IsPaused())
    Pause();

  if (file.GetPath() == m_currentFile->GetPath() &&
      file.m_lStartOffset > 0 &&
      file.m_lStartOffset == m_currentFile->m_lEndOffset)
  { // continuing on a .cue sheet item - return true to say we'll handle the transistion
    *m_nextFile = file;
    return true;
  }

  // ok, we're good to go on queuing this one up
  CLog::Log(LOGINFO, "GSTPlayer: Queuing next file %s", file.GetPath().c_str());

  *m_nextFile = file;

  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
  return true;
}

//Finished
void GSTPlayer::OnNothingToQueueNotify()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  //nothing to queue, stop playing
  m_bQueueFailed = true;
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

bool GSTPlayer::CloseFile()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  if (IsPaused())
    Pause();

  m_bStopPlaying = true;
  m_bStop = true;

  StopThread();

  gst_element_set_state(m_pGstPlaybin, GST_STATE_NULL);

  m_currentFile->Reset();
  m_nextFile->Reset();

#if defined(HAS_VIDEO_PLAYBACK)
   m_renderManager.UnInit();
#endif
  printf ("umount /tmp/nfs ->\n");
  system("umount /tmp/nfs");
  printf ("umount /tmp/nfs <-\n");
  return true;
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

//Finished
void GSTPlayer::Pause()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  CLog::Log(LOGDEBUG,"GSTPlayer: pause m_bplaying: %d", m_bIsPlaying);
  if (!m_bIsPlaying || !m_pGstPlaybin) {
    printf("%s:%s[%d] <<-\n", __FILE__, __func__, __LINE__);
    return;
  }


  if (m_bPaused)
    gst_element_set_state (m_pGstPlaybin, GST_STATE_PLAYING);
  else
    gst_element_set_state (m_pGstPlaybin, GST_STATE_PAUSED);
  
  m_bPaused = !m_bPaused;
  
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

int64_t GSTPlayer::GetTotalTime()
{
  //d1printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  int64_t total;
  gint64 len;
  GstFormat fmt = GST_FORMAT_TIME; //Returns time in nanosecs
#if GST_VERSION_MAJOR < 1
  gst_element_query_duration(m_pGstPlaybin, &fmt, &len);
#else
  gst_element_query_duration(m_pGstPlaybin, fmt, &len);
#endif
  total = len / 1000000;


  if (m_currentFile->m_lEndOffset)
    total = m_currentFile->m_lEndOffset * 1000 / 75;
  if (m_currentFile->m_lStartOffset)
    total -= m_currentFile->m_lStartOffset * 1000 / 75;

  m_duration_ms = total;

  //d1printf("%s:%s[%d] <- len=%lld\n", __FILE__, __func__, __LINE__, total);
  return total;
}


float GSTPlayer::GetPercentage()
{
  //d1printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  float percent = (GetTime() * 100.0f) / GetTotalTime();
  //d1printf("%s:%s[%d] <- percent=%f%%\n", __FILE__, __func__, __LINE__, percent);
  return percent;
}

int64_t GSTPlayer::GetTime()
{
  //d1printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  int64_t ret;
  int64_t timeplus;
  gint64 pos;
  GstFormat fmt = GST_FORMAT_TIME; //Returns time in nanosecs
#if GST_VERSION_MAJOR < 1
  gst_element_query_position(m_pGstPlaybin, &fmt, &pos);
#else
  gst_element_query_position(m_pGstPlaybin, fmt, &pos);
#endif
  timeplus = pos / 1000000;

  ret = m_timeOffset + timeplus - m_currentFile->m_lStartOffset * 1000 / 75;

  m_elapsed_ms = ret;

  //d1printf("%s:%s[%d] <- pos=%lld\n", __FILE__, __func__, __LINE__, ret);
  return ret;
}

bool GSTPlayer::CanSeek()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  printf("%s:%s[%d] <- %d\n", __FILE__, __func__, __LINE__, GetTotalTime() > 0);
  return GetTotalTime() > 0;
}

void GSTPlayer::Seek(bool bPlus, bool bLargeStep)
{
  printf("%s:%s[%d] -> bPlus=%d bLargeStep=%d\n", __FILE__, __func__, __LINE__, bPlus, bLargeStep);

#if 0
  // try chapter seeking first, chapter_index is ones based.
  int chapter_index = GetChapter();
  if (bLargeStep)
  {
    // seek to next chapter
    if (bPlus && (chapter_index < GetChapterCount()))
    {
      SeekChapter(chapter_index + 1);
      return;
    }
    // seek to previous chapter
    if (!bPlus && chapter_index)
    {
      SeekChapter(chapter_index - 1);
      return;
    }
  }
#endif

  int64_t seek_ms;
/*  if (g_advancedSettings.m_videoUseTimeSeeking)
  {
    if (bLargeStep && (GetTotalTime() > (2000 * g_advancedSettings.m_videoTimeSeekForwardBig)))
      seek_ms = bPlus ? g_advancedSettings.m_videoTimeSeekForwardBig : g_advancedSettings.m_videoTimeSeekBackwardBig;
    else
      seek_ms = bPlus ? g_advancedSettings.m_videoTimeSeekForward    : g_advancedSettings.m_videoTimeSeekBackward;
    // convert to milliseconds
    seek_ms *= 1000;
    seek_ms += m_elapsed_ms;
  }
  else
  {
    float percent;
    if (bLargeStep)
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForwardBig : g_advancedSettings.m_videoPercentSeekBackwardBig;
    else
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForward    : g_advancedSettings.m_videoPercentSeekBackward;
    percent /= 100.0f;
    percent += (float)m_elapsed_ms/(float)m_duration_ms;
    // convert to milliseconds
    seek_ms = m_duration_ms * percent;
  }*/

  // handle stacked videos, dvdplayer does it so we do it too.
  if (g_application.CurrentFileItem().IsStack() &&
    (seek_ms > m_duration_ms || seek_ms < 0))
  {
    CLog::Log(LOGDEBUG, "CAMLPlayer::Seek: In mystery code, what did I do");
    g_application.SeekTime((seek_ms - m_elapsed_ms) * 0.001 + g_application.GetTime());
    // warning, don't access any object variables here as
    // the object may have been destroyed
    return;
  }

  if (seek_ms <= 1000)
    seek_ms = 1000;

  if (seek_ms > m_duration_ms)
    seek_ms = m_duration_ms;

  // do seek here
  CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetPlayerInfoProvider().SetDisplayAfterSeek(100000);
  SeekTime(seek_ms);
  m_callback.OnPlayBackSeek((int)seek_ms, (int)(seek_ms - m_elapsed_ms));
  CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetPlayerInfoProvider().SetDisplayAfterSeek();
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

void GSTPlayer::SeekTime(int64_t seek_ms)
{
  printf("%s:%s[%d] -> seek_ms=%ld\n", __FILE__, __func__, __LINE__, seek_ms);
  // we cannot seek if paused
  if (m_bPaused)
    return;

  if (seek_ms <= 0)
    seek_ms = 100;

  // seek here
    // player_timesearch is nanoseconds (int64).
    gst_element_seek (m_pGstPlaybin, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
        GST_SEEK_TYPE_SET, seek_ms*1000000,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
    //WaitForSearchOK(5000);
    //WaitForPlaying(5000);
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

void GSTPlayer::Process()
{
  CLog::Log(LOGDEBUG, "GSTPlayer: Thread started");
  
  
  if (m_startEvent.WaitMSec(100))
  {
    m_startEvent.Reset();
    do
    {
      if (!m_bPaused)
      {
      }
      else
      {
        
      }
      Sleep(100);
    }
    while (!m_bStopPlaying && m_bIsPlaying && !m_bStop);

    CLog::Log(LOGINFO, "GSTPlayer: End of playback reached");
    m_bIsPlaying = false;
    if (!m_bStopPlaying && !m_bStop)
      m_callback.OnPlayBackEnded();
    else
      m_callback.OnPlayBackStopped();
  }
  CLog::Log(LOGDEBUG, "GSTPlayer: Thread end");
}

void GSTPlayer::SetVolume(long nVolume)
{
  printf("%s:%s[%d] -> nVolume=%ld\n", __FILE__, __func__, __LINE__, nVolume);
  //-6000 = mute, 0 is max
  //64 = mute, 0 is max
  FILE* fp = fopen("/proc/stb/avs/0/volume", "w");
  fprintf(fp, "%d", (int)((nVolume*-64)/6000.0));
  fclose(fp);
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

float GSTPlayer::GetActualFPS()
{
  //d1printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  FILE* fp = fopen("/proc/stb/vmpeg/0/framerate", "r");
  int video_fps_tmp = 0;
  float video_fps = 0.0f;
  fscanf(fp, "%x", &video_fps_tmp);
  fclose(fp);
  video_fps = video_fps_tmp / 1000.0f;
  
  //d1printf("%s:%s[%d] <- video_fps=%f\n", __FILE__, __func__, __LINE__, video_fps);
  return video_fps;
}

int GSTPlayer::GetPictureWidth()
{
  //d1printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  FILE* fp = fopen("/proc/stb/vmpeg/0/xres", "r");
  int video_width = 0;
  fscanf(fp, "%x", &video_width);
  fclose(fp);
  //d1printf("%s:%s[%d] <- video_width=%d\n", __FILE__, __func__, __LINE__, video_width);
  return video_width;
}

int GSTPlayer::GetPictureHeight()
{
  //d1printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  FILE* fp = fopen("/proc/stb/vmpeg/0/yres", "r");
  int video_height = 0;
  fscanf(fp, "%x", &video_height);
  fclose(fp);
  //d1printf("%s:%s[%d] <- video_height=%d\n", __FILE__, __func__, __LINE__, video_height);
  return video_height;
}

void GSTPlayer::gstBusCall(GstMessage *msg)
{
  //d2printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  if (!msg) {
    //d2printf("%s:%s[%d] <<-\n", __FILE__, __func__, __LINE__);
    return;
  }
  
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  
  gchar *sourceName;
  GstObject *source;
  source = GST_MESSAGE_SRC(msg);
  if (!GST_IS_OBJECT(source)) return;
  sourceName = gst_object_get_name(source);

  switch (GST_MESSAGE_TYPE (msg))
  {
    case GST_MESSAGE_EOS:
      m_bIsPlaying = false;
      break;
    case GST_MESSAGE_STATE_CHANGED:
    {
      if(GST_MESSAGE_SRC(msg) != GST_OBJECT(m_pGstPlaybin))
        break;
      GstState old_state, new_state;
      gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
      if(old_state == new_state)
        break;
      printf("%s:%s[%d] state transition %s -> %s\n", __FILE__, __func__, __LINE__, 
        gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
      
      GstStateChange transition = (GstStateChange)GST_STATE_TRANSITION(old_state, new_state);
      switch(transition)
      {
        case GST_STATE_CHANGE_NULL_TO_READY:
        case GST_STATE_CHANGE_READY_TO_PAUSED:
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        case GST_STATE_CHANGE_PAUSED_TO_READY:
        case GST_STATE_CHANGE_READY_TO_NULL:
        default:
        break;
      }
      break;
    }
    case GST_MESSAGE_ERROR:
      break;
    case GST_MESSAGE_INFO:
      break;
    case GST_MESSAGE_TAG:
#if 0
      GstTagList *tags, *result;
      gst_message_parse_tag(msg, &tags);
      
      result = gst_tag_list_merge(m_stream_tags, tags, GST_TAG_MERGE_REPLACE);
      if (result)
      {
        if (m_stream_tags)
          gst_tag_list_free(m_stream_tags);
        m_stream_tags = result;
      }
#endif
      
#if 0
      const GValue *gv_image = gst_tag_list_get_value_index(tags, GST_TAG_IMAGE, 0);
      if ( gv_image )
      {
        GstBuffer *buf_image;
        buf_image = gst_value_get_buffer (gv_image);
        int fd = open("/tmp/.id3coverart", O_CREAT|O_WRONLY|O_TRUNC, 0644);
        if (fd >= 0)
        {
          int ret = write(fd, GST_BUFFER_DATA(buf_image), GST_BUFFER_SIZE(buf_image));
          close(fd);
          eDebug("eServiceMP3::/tmp/.id3coverart %d bytes written ", ret);
        }
      }
#endif
      //gst_tag_list_free(tags);
      break;

    case GST_MESSAGE_ASYNC_DONE:
    {
      if(GST_MESSAGE_SRC(msg) != GST_OBJECT(m_pGstPlaybin))
        break;

      GstTagList *tags;
      gint i, active_idx, n_video = 0, n_audio = 0, n_text = 0;

      g_object_get (m_pGstPlaybin, "n-video", &n_video, NULL);
      g_object_get (m_pGstPlaybin, "n-audio", &n_audio, NULL);
      g_object_get (m_pGstPlaybin, "n-text", &n_text, NULL);

      if (n_audio > 0)
        m_bHasAudio = true;
      if (n_video > 0)
        m_bHasVideo = true;

      m_AudioStreams.size();

      for (i = 0; i < n_audio; i++)
      {
        AudioStream audio;
        gchar *g_codec, *g_lang;
        GstPad* pad = 0;
        g_signal_emit_by_name (m_pGstPlaybin, "get-audio-pad", i, &pad);
#if GST_VERSION_MAJOR < 1
        GstCaps* caps = gst_pad_get_negotiated_caps(pad);
#else
        GstCaps* caps = gst_pad_get_current_caps(pad);
#endif
        if (!caps)
          continue;
        GstStructure* str = gst_caps_get_structure(caps, 0);
        const gchar *g_type = gst_structure_get_name(str);
        
        //audio.m_AudioType = gstCheckAudioPad(str);
        g_codec = g_strdup(g_type);
        g_lang = g_strdup_printf ("Unknown");
        g_signal_emit_by_name (m_pGstPlaybin, "get-audio-tags", i, &tags);
#if GST_VERSION_MAJOR < 1
        if ( tags && gst_is_tag_list(tags) )
#else
        if ( tags && GST_IS_TAG_LIST(tags) )
#endif
        {
          gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &g_codec);
          gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &g_lang);
          gst_tag_list_free(tags);
        }
        audio.m_sLanguageCode = g_lang;
        audio.m_sCodec = g_codec;

        m_AudioStreams.push_back(audio);

        g_free (g_lang);
        g_free (g_codec);
        gst_caps_unref(caps);
      }

      break;
    }
    case GST_MESSAGE_ELEMENT:
    {
      const GstStructure *msgstruct = gst_message_get_structure(msg);
      if (msgstruct)
      {
        //if ( gst_is_missing_plugin_message(msg) )
        //  break;

        const gchar *eventname = gst_structure_get_name(msgstruct);
        if ( eventname == NULL )
          break;

        if (!strcmp(eventname, "eventSizeChanged") || !strcmp(eventname, "eventSizeAvail"))
        {
          gst_structure_get_int (msgstruct, "aspect_ratio", &m_iVideoAspect);
          gst_structure_get_int (msgstruct, "width", &m_iVideoWidth);
          gst_structure_get_int (msgstruct, "height", &m_iVideoHeight);
          if (strstr(eventname, "Changed"))
          {
            m_evVideoSizeChanged = 1;
            printf("%s:%s[%d] VideoSize Changed a=%d w=%d h=%d\n", __FILE__, __func__, __LINE__, m_iVideoAspect, m_iVideoWidth, m_iVideoHeight);
            if (m_evVideoFramerateChanged)
            {
     VideoPicture picture = {};
     picture.iWidth = m_iVideoWidth;
     picture.iHeight = m_iVideoHeight;
     picture.iDisplayWidth = 1280;
     picture.iDisplayHeight = 720;

               m_renderManager.Configure(picture, m_iVideoFramerate/1000.0, 0);
//              KODI::MESSAGING::CApplicationMessenger::GetInstance().SwitchToFullscreen();
            }
          }
        }
        else if (!strcmp(eventname, "eventFrameRateChanged") || !strcmp(eventname, "eventFrameRateAvail"))
        {
          gst_structure_get_int (msgstruct, "frame_rate", &m_iVideoFramerate);
          if (strstr(eventname, "Changed"))
          {
            m_evVideoFramerateChanged = 1;
            printf("%s:%s[%d] VideoFramerate Changed f=%i\n", __FILE__, __func__, __LINE__, m_iVideoFramerate);
            
            if (m_evVideoSizeChanged)
            {
     VideoPicture picture = {};
     picture.iWidth = m_iVideoWidth;
     picture.iHeight = m_iVideoHeight;
     picture.iDisplayWidth = 1280;
     picture.iDisplayHeight = 720;

               m_renderManager.Configure(picture, m_iVideoFramerate/1000.0, 0);
//              KODI::MESSAGING::CApplicationMessenger::GetInstance().SwitchToFullscreen();
            }
          }
        }
        else if (!strcmp(eventname, "eventProgressiveChanged") || !strcmp(eventname, "eventProgressiveAvail"))
        {
          gst_structure_get_int (msgstruct, "progressive", &m_iVideoProgressive);
          if (strstr(eventname, "Changed"))
          {
            m_evVideoProgressiveChanged = 1;
            printf("%s:%s[%d] VideoProgressive Changed p=%i\n", __FILE__, __func__, __LINE__, m_iVideoProgressive);
          }
        }
        else if (!strcmp(eventname, "redirect"))
        {
           const char *uri = gst_structure_get_string(msgstruct, "new-location");
           gst_element_set_state (m_pGstPlaybin, GST_STATE_NULL);
           g_object_set(G_OBJECT (m_pGstPlaybin), "uri", uri, NULL);
           gst_element_set_state (m_pGstPlaybin, GST_STATE_PLAYING);
        }
      }
      break;
    }

    default:
      break;
  }
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

void GSTPlayer::handleMessage(GstMessage *msg)
{
  //d2printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_STATE_CHANGED && GST_MESSAGE_SRC(msg) != GST_OBJECT(m_pGstPlaybin))
  {
    gst_message_unref(msg);
    //d2printf("%s:%s[%d] <<-\n", __FILE__, __func__, __LINE__);
    return;
  }
  gstBusCall(msg);
  //d2printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}

GstBusSyncReply GSTPlayer::gstBusSyncHandler(GstBus *bus, GstMessage *message, gpointer user_data)
{
  //d2printf("%s:%s[%d]\n", __FILE__, __func__, __LINE__);
  GSTPlayer *_this = (GSTPlayer*)user_data;
  if (_this) _this->handleMessage(message);
  
  //d2printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
  return GST_BUS_DROP;
}

int GSTPlayer::GetAudioStreamCount()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
  return m_AudioStreams.size();
}

int GSTPlayer::GetAudioStream()
{
  printf("%s:%s[%d] ->\n", __FILE__, __func__, __LINE__);
  if (m_iCurrentAudioStream == -1)
    g_object_get (G_OBJECT (m_pGstPlaybin), "current-audio", &m_iCurrentAudioStream, NULL);
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
  return m_iCurrentAudioStream;
}

void GSTPlayer::SetAudioStream(int iStream)
{
  printf("%s:%s[%d] -> iStream=%d\n", __FILE__, __func__, __LINE__, iStream);
  if (iStream >= m_AudioStreams.size()) {
    printf("%s:%s[%d] <<-\n", __FILE__, __func__, __LINE__);
    return;
  }

  int current_audio;
  g_object_set (G_OBJECT (m_pGstPlaybin), "current-audio", iStream, NULL);
  g_object_get (G_OBJECT (m_pGstPlaybin), "current-audio", &current_audio, NULL);
  if ( current_audio == iStream )
  {
    m_iCurrentAudioStream = iStream;
  }
  printf("%s:%s[%d] <-\n", __FILE__, __func__, __LINE__);
}
