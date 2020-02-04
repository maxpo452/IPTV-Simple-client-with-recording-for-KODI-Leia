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
#include "libexecstream/exec-stream.h"

using namespace ADDON;

class PVRRecorderThread : P8PLATFORM::CThread
{
public: 
  PVRRecorderThread(PVRIptvChannel &currChannel, PVRDvrTimer &currTimer);
  virtual ~PVRRecorderThread(void);

  virtual void StopThread(bool bWait = true);

public:
  virtual void *Process(void);

protected:
  virtual void CorrectDurationFLVFile(const std::string &videoFile, const double &duration);

private:
  FILE*          t_proc;
  exec_stream_t  e_Stream;
  double         t_duration;
  PVRDvrTimer    t_currTimer;
  PVRIptvChannel t_currChannel;
  time_t         t_startRecTime;
};