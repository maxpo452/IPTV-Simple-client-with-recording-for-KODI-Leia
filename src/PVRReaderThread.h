#pragma once
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
#include <string>

#include "PVRIptvData.h"
#include "PVRDvrData.h"
#include "PVRSchedulerThread.h"
#include "p8-platform/util/StdString.h"
#include "p8-platform/threads/threads.h"

using namespace ADDON;

class PVRReaderThread
{
public: 
  PVRReaderThread(int recid, const std::string &filePath, time_t duration);
  virtual ~PVRReaderThread(void);

  virtual bool    StartThread();
  virtual void    StopThread(bool bWait = true);
  virtual void    OnPlay(); 
  virtual ssize_t ReadThread(unsigned char *buffer, unsigned int size);
  virtual int64_t SeekThread(long long position, int whence);
  virtual int64_t RecordingId();
  virtual int64_t Position();
  virtual int64_t Length();

private:
  int          x_recid;
  std::string  x_filePath;
  void        *x_fileHandle;

  time_t       x_end;
  time_t       x_nextReopen;
  bool         x_fastReopen;

  bool         x_playback;
  uint64_t     x_pos;
  uint64_t     x_len; 
};
