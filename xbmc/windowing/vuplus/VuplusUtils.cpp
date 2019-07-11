/*
 *  Copyright (C) 2011-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

CVuplusUtils::CVuplusUtils()
{
}

CVuplusUtils::~CVuplusUtils()
{
}

bool CVuplusUtils::GetNativeResolution(RESOLUTION_INFO *res) const
{
  *res = m_desktopRes;

  return true;
}

bool CVuplusUtils::SetNativeResolution(const RESOLUTION_INFO res, EGLSurface m_nativeWindow)
{
  *res = m_desktopRes;
  return true;
}

bool CVuplusUtils::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  resolutions.clear();

  m_desktopResAll[0].iScreen      = 0;
  m_desktopResAll[0].bFullScreen  = true;
  m_desktopResAll[0].iWidth       = 1280;
  m_desktopResAll[0].iHeight      = 720;
  m_desktopResAll[0].iScreenWidth = 1280;
  m_desktopResAll[0].iScreenHeight= 720;
  m_desktopResAll[0].dwFlags      =  D3DPRESENTFLAG_PROGRESSIVE;
  m_desktopResAll[0].fRefreshRate = 50;
  m_desktopResAll[0].strMode = StringUtils::Format("%dx%d", 1280, 720);
  m_desktopResAll[0].strMode = StringUtils::Format("%s @ %.2f%s - Full Screen", m_desktopRes.strMode.c_str(), (float)50,m_desktopRes.dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

  m_desktopResAll[0].iSubtitles   = (int)(0.965 * m_desktopResAll[0].iHeight);

  CLog::Log(LOGDEBUG, "EGL initial desktop resolution %s\n", m_desktopResAll[0].strMode.c_str());

  resolutions.push_back(m_desktopResAll[0]);

  m_desktopResAll[1].iScreen      = 0;
  m_desktopResAll[1].bFullScreen  = true;
  m_desktopResAll[1].iWidth       = 1280;
  m_desktopResAll[1].iHeight      = 720;
  m_desktopResAll[1].iScreenWidth = 1280;
  m_desktopResAll[1].iScreenHeight= 720;
  m_desktopResAll[1].dwFlags      =  D3DPRESENTFLAG_PROGRESSIVE;

  m_desktopResAll[1].dwFlags      |=  D3DPRESENTFLAG_MODE3DSBS;
  m_desktopResAll[1].fRefreshRate = 50;
  m_desktopResAll[1].strMode = StringUtils::Format("%dx%d", 1280, 720);
  m_desktopResAll[1].strMode = StringUtils::Format("%s @ %.2f%s - Full Screen 3DSBS", m_desktopResAll[1].strMode.c_str(), (float)50,m_desktopResAll[1].dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

  m_desktopResAll[1].iSubtitles   = (int)(0.965 * m_desktopResAll[1].iHeight);

  CLog::Log(LOGDEBUG, "EGL initial desktop resolution %s\n", m_desktopResAll[1].strMode.c_str());

  resolutions.push_back(m_desktopResAll[1]);

  m_desktopResAll[2].iScreen      = 0;
  m_desktopResAll[2].bFullScreen  = true;
  m_desktopResAll[2].iWidth       = 1280;
  m_desktopResAll[2].iHeight      = 720;
  m_desktopResAll[2].iScreenWidth = 1280;
  m_desktopResAll[2].iScreenHeight= 720;
  m_desktopResAll[2].dwFlags      =  D3DPRESENTFLAG_PROGRESSIVE;

  m_desktopResAll[2].dwFlags      |=  D3DPRESENTFLAG_MODE3DTB;
  m_desktopResAll[2].fRefreshRate = 50;
  m_desktopResAll[2].strMode = StringUtils::Format("%dx%d", 1280, 720);
  m_desktopResAll[2].strMode = StringUtils::Format("%s @ %.2f%s - Full Screen 3DTB", m_desktopResAll[2].strMode.c_str(), (float)50,m_desktopResAll[2].dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");

  m_desktopResAll[2].iSubtitles   = (int)(0.965 * m_desktopResAll[2].iHeight);

  CLog::Log(LOGDEBUG, "EGL initial desktop resolution %s\n", m_desktopResAll[2].strMode.c_str());

  resolutions.push_back(m_desktopResAll[2]);
  m_desktopRes = m_desktopResAll[0];

  return true;
}
