/*
 *      Copyright (C) 2018 Gonzalo Vega
 *      https://github.com/gonzalo-hvega/xbmc-pvr-iptvsimple/
 *
 *      Copyright (C) 2015 Radek Kubera
 *      http://github.com/afedchin/xbmc-addon-iptvsimple/
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

#include <map>
#include <iostream>
#include <fstream>
#include <string>

#include <unistd.h>
#include "PVRReaderThread.h"
#include "PVRPlayList.h"
#include "PVRUtils.h"
#include "p8-platform/util/StringUtils.h"

#define REOPEN_INTERVAL      30
#define REOPEN_INTERVAL_FAST 10

using namespace ADDON;

PVRReaderThread::PVRReaderThread(int recid, const string &filePath, time_t duration)
{
  XBMC->Log(LOG_NOTICE, "Creating reader thread");

  x_recid      = recid;
  x_filePath   = filePath;
  x_end        = duration;
  x_playback   = false;
  x_fileHandle = XBMC->OpenFile(x_filePath.c_str(), 0);
  x_len        = XBMC->GetFileLength(x_fileHandle);
  x_pos        = 0;
  x_nextReopen = time(NULL) + REOPEN_INTERVAL;
  x_fastReopen = false;
}

PVRReaderThread::~PVRReaderThread(void)
{
  XBMC->Log(LOG_DEBUG, "Closing thread %s", x_filePath.c_str());

  if (x_fileHandle)
  {
    XBMC->CloseFile(x_fileHandle);
    XBMC->Log(LOG_DEBUG, "Waiting for close reader thread %s", x_filePath.c_str());
  }
}

bool PVRReaderThread::StartThread()
{
  return (x_fileHandle != nullptr);
}

void PVRReaderThread::StopThread(bool bWait /*= true*/)
{
  XBMC->CloseFile(x_fileHandle);

  if (x_fileHandle);
    XBMC->Log(LOG_DEBUG, "Stopping reader thread %s", x_filePath.c_str());

  x_fileHandle = NULL;
}

void PVRReaderThread::OnPlay()
{
  XBMC->Log(LOG_NOTICE, "Reader started, file=%s, duration=%u", x_filePath.c_str(), x_end);

  x_playback = true;
}

ssize_t PVRReaderThread::ReadThread(unsigned char *buffer, unsigned int size)
{
  // check for playback of ongoing recording
  if (x_playback && x_end)
  {
    time_t now = time(NULL);

    if (now > x_nextReopen)
    {
      FORCE_REOPEN:
        // reopen stream
        XBMC->Log(LOG_DEBUG, "Reader is reopening stream...");
        XBMC->CloseFile(x_fileHandle);
        x_fileHandle = XBMC->OpenFile(x_filePath.c_str(), 0);
        x_len = XBMC->GetFileLength(x_fileHandle);
      //XBMC->SeekFile(x_fileHandle, x_pos, SEEK_SET);

        x_nextReopen = now + ((x_fastReopen) ? REOPEN_INTERVAL_FAST : REOPEN_INTERVAL);

        // recording has finished
        if (now > x_end)
          x_end = 0;
    }
    else if (x_pos == x_len)
    {
      // in case we reached the end we need to wait a little
      int sleep = REOPEN_INTERVAL_FAST + 5;

      if (!x_fastReopen)
        sleep = std::min(sleep, static_cast<int>(x_nextReopen - now + 1));
      
      XBMC->Log(LOG_DEBUG, "Reader reached end of recording. Sleeping %d secs", sleep);

      P8PLATFORM::CEvent::Sleep(sleep * 1000);
      now += sleep;

      x_fastReopen = true;
      goto FORCE_REOPEN;
    }
  }

  // read to XBMC
  ssize_t read = XBMC->ReadFile(x_fileHandle, buffer, size);
  x_pos += read;
  return read;
}

int64_t PVRReaderThread::SeekThread(long long position, int whence)
{
  int64_t ret = XBMC->SeekFile(x_fileHandle, position, whence);

  // for unknown reason seek sometimes doesn't return the correct position
  // so let's sync with the underlaying implementation
  x_pos = XBMC->GetFilePosition(x_fileHandle);
  x_len = XBMC->GetFileLength(x_fileHandle);
  return ret;
}

int64_t PVRReaderThread::RecordingId()
{
  return x_recid;
}

int64_t PVRReaderThread::Position()
{
  return x_pos;
}

int64_t PVRReaderThread::Length()
{
  return x_len;
}