#pragma once

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

#include "cores/IPlayer.h"
#include "threads/Thread.h"
#include "threads/SharedSection.h"
#include "cores/VideoPlayer/VideoRenderers/RenderManager.h"

#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>

#define STATUS_NO_FILE  0
#define STATUS_QUEUING  1
#define STATUS_QUEUED   2
#define STATUS_PLAYING  3
#define STATUS_ENDING   4
#define STATUS_ENDED    5

typedef enum {
  GST_PLAY_FLAG_VIDEO         = (1 << 0),
  GST_PLAY_FLAG_AUDIO         = (1 << 1),
  GST_PLAY_FLAG_TEXT          = (1 << 2),
  GST_PLAY_FLAG_VIS           = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOLUME   = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO  = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO  = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD      = (1 << 7),
  GST_PLAY_FLAG_BUFFERING     = (1 << 8)
} GstPlayFlags;

class GSTPlayer : public IPlayer, public CThread
{
public:
  GSTPlayer(IPlayerCallback& callback, CRenderManager& renderManager);
  virtual ~GSTPlayer();

  virtual bool OpenFile(const CFileItem& file, const CPlayerOptions &options);
  virtual bool QueueNextFile(const CFileItem &file);
  virtual void OnNothingToQueueNotify();
  virtual bool CloseFile();
  virtual bool IsPlaying() const { return m_bIsPlaying; }
  virtual void Pause();
  virtual bool IsPaused() const { return m_bPaused; }
  virtual bool HasVideo() const { return m_bHasVideo; }
  virtual bool HasAudio() const { return m_bHasAudio; }

  virtual bool CanSeek();
  virtual void Seek(bool bPlus = true, bool bLargeStep = false);
  virtual void SeekTime(int64_t iTime = 0);
  
  virtual void SetVolume(long nVolume);

/*  virtual void GetAudioInfo( CStdString& strAudioInfo) {}
  virtual void GetVideoInfo( CStdString& strVideoInfo) {}
  virtual void GetGeneralInfo( CStdString& strVideoInfo) {}*/
  virtual void Update(bool bPauseDrawing = false) {}
  
  virtual int64_t GetTotalTime();
  virtual int64_t GetTime();
  virtual float GetPercentage();

  virtual int  GetAudioStreamCount();
  virtual int  GetAudioStream();
  virtual void SetAudioStream(int iStream);

  virtual float GetActualFPS();
  virtual int   GetPictureWidth();
  virtual int   GetPictureHeight();
protected:
  CRenderManager& m_renderManager;

  virtual void OnStartup() {}
  virtual void Process();
  virtual void OnExit();

  struct AudioStream {
    GstPad* m_Pad;
//    audiotype_t m_AudioType;
    char* m_sLanguageCode;
    char* m_sCodec;
  };

  GstElement *m_pGstPlaybin;
  
  bool m_bPaused;
  bool m_bIsPlaying;
  bool m_bQueueFailed;
  bool m_bStopPlaying;
  int m_iSpeed;   // current playing speed
  
  bool m_bHasAudio;
  bool m_bHasVideo;

  std::vector<AudioStream> m_AudioStreams;
  //int m_iAudioStreamCount;
  int m_iCurrentAudioStream;

  int m_iVideoAspect;
  int m_iVideoWidth;
  int m_iVideoHeight;
  int m_iVideoFramerate;
  int m_iVideoProgressive;

  int m_evVideoSizeChanged;
  int m_evVideoFramerateChanged;
  int m_evVideoProgressiveChanged;

  CEvent m_startEvent;

private:
  void gstBusCall(GstMessage *msg);
  void handleMessage(GstMessage *msg);
  static GstBusSyncReply gstBusSyncHandler(GstBus *bus, GstMessage *message, gpointer user_data);

  int64_t m_timeOffset;
  int64_t                 m_elapsed_ms;
  int64_t                 m_duration_ms;

  // our file
  CFileItem*        m_currentFile;
  CFileItem*        m_nextFile;
};

