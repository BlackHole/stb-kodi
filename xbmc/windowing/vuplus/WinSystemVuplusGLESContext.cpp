/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoSyncAML.h"
#include "WinSystemVuplusGLESContext.h"
#include "utils/log.h"
#include "threads/SingleLock.h"

std::unique_ptr<CWinSystemBase> CWinSystemBase::CreateWinSystem()
{
  std::unique_ptr<CWinSystemBase> winSystem(new CWinSystemVuplusGLESContext());
  return winSystem;
}

bool CWinSystemVuplusGLESContext::InitWindowSystem()
{
  if (!CWinSystemVuplus::InitWindowSystem())
  {
    return false;
  }

  if (!m_pGLContext.CreateDisplay(m_nativeDisplay))
  {
    return false;
  }

  if (!m_pGLContext.InitializeDisplay(EGL_OPENGL_ES_API))
  {
    return false;
  }

  if (!m_pGLContext.ChooseConfig(EGL_OPENGL_ES2_BIT))
  {
    return false;
  }

  CEGLAttributesVec contextAttribs;
  contextAttribs.Add({{EGL_CONTEXT_CLIENT_VERSION, 2}});

  if (!m_pGLContext.CreateContext(contextAttribs))
  {
    return false;
  }

  return true;
}

bool CWinSystemVuplusGLESContext::CreateNewWindow(const std::string& name,
                                               bool fullScreen,
                                               RESOLUTION_INFO& res)
{
  m_pGLContext.DestroySurface();

  if (!CWinSystemVuplus::DestroyWindow())
  {
    return false;
  }

  if (!CWinSystemVuplus::CreateNewWindow(name, fullScreen, res))
  {
    return false;
  }

  if (!m_pGLContext.CreateSurface(m_nativeWindow))
  {
    return false;
  }

  if (!m_pGLContext.BindContext())
  {
    return false;
  }

  if (!m_delayDispReset)
  {
    CSingleLock lock(m_resourceSection);
    // tell any shared resources
    for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); ++i)
      (*i)->OnResetDisplay();
  }

  return true;
}

bool CWinSystemVuplusGLESContext::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  CRenderSystemGLES::ResetRenderSystem(newWidth, newHeight);
  return true;
}

bool CWinSystemVuplusGLESContext::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  CreateNewWindow("", fullScreen, res);
  CRenderSystemGLES::ResetRenderSystem(res.iWidth, res.iHeight);
  return true;
}

void CWinSystemVuplusGLESContext::SetVSyncImpl(bool enable)
{
  if (!m_pGLContext.SetVSync(enable))
  {
    CLog::Log(LOGERROR, "%s,Could not set egl vsync", __FUNCTION__);
  }
}

void CWinSystemVuplusGLESContext::PresentRenderImpl(bool rendered)
{
  if (m_delayDispReset && m_dispResetTimer.IsTimePast())
  {
    m_delayDispReset = false;
    CSingleLock lock(m_resourceSection);
    // tell any shared resources
    for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); ++i)
      (*i)->OnResetDisplay();
  }
  if (!rendered)
    return;

  // Ignore errors - eglSwapBuffers() sometimes fails during modeswaps on AML,
  // there is probably nothing we can do about it
  m_pGLContext.TrySwapBuffers();
}

EGLDisplay CWinSystemVuplusGLESContext::GetEGLDisplay() const
{
  return m_pGLContext.GetEGLDisplay();
}

EGLSurface CWinSystemVuplusGLESContext::GetEGLSurface() const
{
  return m_pGLContext.GetEGLSurface();
}

EGLContext CWinSystemVuplusGLESContext::GetEGLContext() const
{
  return m_pGLContext.GetEGLContext();
}

EGLConfig  CWinSystemVuplusGLESContext::GetEGLConfig() const
{
  return m_pGLContext.GetEGLConfig();
}

std::unique_ptr<CVideoSync> CWinSystemVuplusGLESContext::GetVideoSync(void *clock)
{
  std::unique_ptr<CVideoSync> pVSync(new CVideoSyncAML(clock));
  return pVSync;
}

