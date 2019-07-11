#pragma once

/*
 *      Copyright (C) 2011-2012 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "EGLNativeType.h"

#include <vector>
#include <string>

class CEGLNativeTypeVuplus : public CEGLNativeType
{
public:
  CEGLNativeTypeVuplus()
  {
	  m_nativeWindow = 0;
	  m_nativeDisplay = 0;
  };
  virtual ~CEGLNativeTypeVuplus() {};
  virtual std::string GetNativeName() const { return "vuplus"; };
nonserve	virtual bool  CheckCompatibility();
nonserve	virtual void  Initialize();
nonserve	virtual void  Destroy();
  virtual int   GetQuirks() { return EGL_QUIRK_NONE; };

wip		virtual bool  CreateNativeDisplay(); -->  CWinSystemVuplus::InitWindowSystem
wip  virtual bool  CreateNativeWindow(); --> CreateNewWindow
forsenonserve	  virtual bool  GetNativeDisplay(XBNativeDisplayType **nativeDisplay) const;
forsenonserve	  virtual bool  GetNativeWindow(XBNativeWindowType **nativeWindow) const;

wip  virtual bool  DestroyNativeWindow(); -->destructor+ destroynativewindow
  virtual bool  DestroyNativeDisplay();

  virtual bool  GetNativeResolution(RESOLUTION_INFO *res) const;
  virtual bool  SetNativeResolution(const RESOLUTION_INFO &res);
ok		virtual bool  ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions);
  virtual bool  GetPreferredResolution(RESOLUTION_INFO *res) const;

  virtual bool  ShowWindow(bool show);
private:
  RESOLUTION_INFO m_desktopRes;
  RESOLUTION_INFO m_desktopResAll[3];
};
