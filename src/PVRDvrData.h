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

#include "p8-platform/util/StdString.h"
#include "client.h"
#include "p8-platform/threads/threads.h"

using namespace ADDON;

class PVRDvrData : public P8PLATFORM::CThread
{
public:
  PVRDvrData(void);
  virtual ~PVRDvrData(void);

  virtual bool      ReLoadTimers(void);
  virtual bool      ReLoadRecordings(void);

  virtual PVR_ERROR AddTimer(const PVR_TIMER &timer);
  virtual PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete);
  virtual PVR_ERROR UpdateTimer(const PVR_TIMER &timer);
  virtual PVR_ERROR GetTimers(ADDON_HANDLE handle);
  virtual int       GetTimersAmount(void);
  virtual PVR_ERROR GetRecordings(ADDON_HANDLE handle);
  virtual int       GetRecordingsAmount(void);
  virtual PVR_ERROR DeleteRecording(const PVR_RECORDING &recording);
  virtual bool      OpenRecordedStream(const PVR_RECORDING &recording);
  virtual void      CloseRecordedStream(void);
  virtual int       ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize);
  virtual long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */);
  virtual long long GetReaderPosition(void);
  virtual long long GetReaderLength(void);
  virtual PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition);
  virtual int       GetRecordingLastPlayedPosition(const PVR_RECORDING &recording);
  virtual PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count);

public:
  virtual std::map<int, PVRDvrTimer> GetTimerData(void);
  virtual bool                       GetChannel(const PVR_TIMER &timer, PVRIptvChannel &myChannel);
  virtual bool                       GetTimer(const PVR_TIMER &timer, PVRDvrTimer &myTimer);
  virtual bool                       DeleteTimer(const PVRDvrTimer &myTimer, bool bForceDelete);
  virtual bool                       UpdateTimer(const PVRDvrTimer &myTimer);
  virtual bool                       RescheduleTimer(const PVRDvrTimer &myTimer);

public:
  virtual std::map<int, PVRDvrRecording> GetRecordingData(void);
  virtual bool                           GetRecording(const PVR_RECORDING &recording, PVRDvrRecording &myRecording);
  virtual bool                           AddRecording(const PVRDvrRecording &myRecording);
  virtual bool                           DeleteRecording(const PVRDvrRecording &myRecording);
  virtual bool                           UpdateRecording(const PVRDvrRecording &myRecording);

public:
  virtual bool SetLock(void);
  virtual void SetUnlock(void);

public:
  virtual void *Process(void);

protected:
  virtual bool        LoadTimers(void);
  virtual bool        StoreTimerData(void);
  virtual std::string GetTimerString(const PVRDvrTimer &myTimer);
  virtual bool        ParseTimerString(const std::string buffStr, PVRDvrTimer &myTimer);

protected:
  virtual bool        LoadRecordings(void);
  virtual bool        StoreRecordingData(void);
  virtual std::string GetRecordingString(const PVRDvrRecording &myRecording);
  virtual bool        ParseRecordingString(std::string buffStr, PVRDvrRecording &myRecording);

private:
  std::map <int, PVRDvrTimer> m_timers;

private:
  std::map <int, PVRDvrRecording> m_recordings;
};