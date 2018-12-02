/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EmuFileWrapper.h"
#include "filesystem/File.h"
#include "threads/SingleLock.h"

CEmuFileWrapper g_emuFileWrapper;

namespace
{

constexpr bool isValidFilePtr(FILE* f)
{
  return (f != nullptr);
}

}
CEmuFileWrapper::CEmuFileWrapper()
{
  memset(m_files, 0, sizeof(m_files));
}

CEmuFileWrapper::~CEmuFileWrapper()
{
  CleanUp();
}

void CEmuFileWrapper::CleanUp()
{
  CSingleLock lock(m_criticalSection);
  for (int i = 0; i < MAX_EMULATED_FILES; i++)
  {
    UnRegisterFileObject(&m_files[i], true);
  }
}

EmuFileObject* CEmuFileWrapper::RegisterFileObject(XFILE::CFile* pFile)
{
  EmuFileObject* object = nullptr;

  CSingleLock lock(m_criticalSection);

  for (int i = 0; i < MAX_EMULATED_FILES; i++)
  {
    if (!m_files[i].file_xbmc)
    {
      // found a free location
      object = &m_files[i];
      object->file_xbmc = pFile;
      object->file_lock = new CCriticalSection();
      break;
    }
  }

  return object;
}

void CEmuFileWrapper::UnRegisterFileObject(EmuFileObject *object, bool free_file)
{
  if (object && object->file_xbmc)
  {
    if (object->file_xbmc && free_file)
    {
      object->file_xbmc->Close();
      delete object->file_xbmc;
    }
    if (object->file_lock)
    {
      delete object->file_lock;
    }
    memset(object, 0, sizeof(*object));
  }
}

void CEmuFileWrapper::UnRegisterFileObjectByDescriptor(int fd)
{
  CSingleLock lock(m_criticalSection);
  UnRegisterFileObject(GetFileObjectByDescriptor(fd), false);
}

void CEmuFileWrapper::UnRegisterFileObjectByStream(FILE* stream)
{
  if (isValidFilePtr(stream))
  {
    CSingleLock lock(m_criticalSection);
    UnRegisterFileObject(GetFileObjectByStream(stream), false);
  }
}

void CEmuFileWrapper::LockFileObjectByDescriptor(int fd)
{
  EmuFileObject* object = GetFileObjectByDescriptor(fd);
  if (object && object->file_xbmc)
  {
    object->file_lock->lock();
  }
}

bool CEmuFileWrapper::TryLockFileObjectByDescriptor(int fd)
{
  EmuFileObject* object = GetFileObjectByDescriptor(fd);
  if (object && object->file_xbmc)
  {
    return object->file_lock->try_lock();
  }
  return false;
}

void CEmuFileWrapper::UnlockFileObjectByDescriptor(int fd)
{
  EmuFileObject* object = GetFileObjectByDescriptor(fd);
  if (object && object->file_xbmc)
  {
    object->file_lock->unlock();
  }
}

EmuFileObject* CEmuFileWrapper::GetFileObjectByDescriptor(int fd)
{
  int i = fd - 0x7000000;
  if (i >= 0 && i < MAX_EMULATED_FILES)
  {
    if (m_files[i].file_xbmc)
    {
      return &m_files[i];
    }
  }
  return nullptr;
}

int CEmuFileWrapper::GetDescriptorByFileObject(EmuFileObject *object)
{
  int i = object - m_files;
  if (i >= 0 && i < MAX_EMULATED_FILES)
  {
    return 0x7000000 + i;
  }

  return -1;
}

EmuFileObject* CEmuFileWrapper::GetFileObjectByStream(FILE* stream)
{
  EmuFileObject *object = (EmuFileObject*) stream;
  if (object >= &m_files[0] || object < &m_files[MAX_EMULATED_FILES])
  {
    if (object->file_xbmc)
    {
      return object;
    }
  }
  return NULL;
}

FILE* CEmuFileWrapper::GetStreamByFileObject(EmuFileObject *object)
{
  return (FILE*) object;
}

XFILE::CFile* CEmuFileWrapper::GetFileXbmcByDescriptor(int fd)
{
  auto object = GetFileObjectByDescriptor(fd);
  if (object != nullptr)
  {
    return object->file_xbmc;
  }
  return nullptr;
}

XFILE::CFile* CEmuFileWrapper::GetFileXbmcByStream(FILE* stream)
{
  if (isValidFilePtr(stream))
  {
    EmuFileObject* object = GetFileObjectByStream(stream);
    if (object != NULL)
    {
      return object->file_xbmc;
    }
  }
  return nullptr;
}

int CEmuFileWrapper::GetDescriptorByStream(FILE* stream)
{
  return GetDescriptorByFileObject(GetFileObjectByStream(stream));
}

FILE* CEmuFileWrapper::GetStreamByDescriptor(int fd)
{
  return GetStreamByFileObject(GetFileObjectByDescriptor(fd));
}

bool CEmuFileWrapper::DescriptorIsEmulatedFile(int fd)
{
  return GetFileObjectByDescriptor(fd) != NULL;
}

bool CEmuFileWrapper::StreamIsEmulatedFile(FILE* stream)
{
  return GetFileObjectByStream(stream) != NULL;
}
