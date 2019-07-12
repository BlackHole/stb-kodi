/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "EGL/egl.h"
#include "windowing/Resolution.h"
#include <vector>

class CVuplusUtils
{
public:
  CVuplusUtils();
  virtual ~CVuplusUtils();
  virtual bool GetNativeResolution(RESOLUTION_INFO *res) const;
  virtual bool SetNativeResolution(const RESOLUTION_INFO res, EGLSurface m_nativeWindow);
  virtual bool ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions);
private:
  RESOLUTION_INFO m_desktopRes;
  RESOLUTION_INFO m_desktopResAll[3];
};