/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <stdio.h>

#include "threads/CriticalSection.h"

//#if defined(TARGET_POSIX) && !defined(TARGET_DARWIN) && !defined(TARGET_FREEBSD) && !defined(TARGET_ANDROID) && !defined(__UCLIBC__)
//#define _file _fileno
//#elif defined(__UCLIBC__)
//#define _file __filedes
//#endif

#define MAX_EMULATED_FILES    50
//#define FILE_WRAPPER_OFFSET   0x00000200

namespace XFILE
{
  class CFile;
}

typedef struct stEmuFileObject
{
  XFILE::CFile*  file_xbmc;
  CCriticalSection *file_lock;
  int mode;
  int fd;
} EmuFileObject;

class CEmuFileWrapper
{
public:
  CEmuFileWrapper();
  ~CEmuFileWrapper();

  /**
   * Only to be called when shutting down xbmc
   */
  void CleanUp();

  EmuFileObject* RegisterFileObject(XFILE::CFile* pFile);
  void UnRegisterFileObject(EmuFileObject*, bool free_file);
  void UnRegisterFileObjectByDescriptor(int fd);
  void UnRegisterFileObjectByStream(FILE* stream);
  void LockFileObjectByDescriptor(int fd);
  bool TryLockFileObjectByDescriptor(int fd);
  void UnlockFileObjectByDescriptor(int fd);
  EmuFileObject* GetFileObjectByDescriptor(int fd);
  int GetDescriptorByFileObject(EmuFileObject*);
  EmuFileObject* GetFileObjectByStream(FILE* stream);
  FILE* GetStreamByFileObject(EmuFileObject*);
  XFILE::CFile* GetFileXbmcByDescriptor(int fd);
  XFILE::CFile* GetFileXbmcByStream(FILE* stream);
  int GetDescriptorByStream(FILE* stream);
  FILE* GetStreamByDescriptor(int fd);
  bool DescriptorIsEmulatedFile(int fd);
  bool StreamIsEmulatedFile(FILE* stream);
private:
  EmuFileObject m_files[MAX_EMULATED_FILES];
  CCriticalSection m_criticalSection;
};

extern CEmuFileWrapper g_emuFileWrapper;

