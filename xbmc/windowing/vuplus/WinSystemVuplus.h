/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "VuplusUtils.h"

class IDispResource;

class CWinSystemVuplus : public CWinSystemBase
{
public:
  CWinSystemVuplus();
  virtual ~CWinSystemVuplus();

  virtual void Register(IDispResource *resource);
  virtual void Unregister(IDispResource *resource);
protected:
  CVuplusUtils *m_vuplus;
  EGLDisplay m_nativeDisplay;
  EGLSurface m_nativeWindow;

};
