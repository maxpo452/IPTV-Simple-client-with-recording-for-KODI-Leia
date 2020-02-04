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

#include <stdlib.h>
#include "PVRSchedulerThread.h"
#include "PVRRecorderThread.h"
#include "PVRReaderThread.h"
#include "PVRUtils.h"

using namespace std;
using namespace ADDON;

extern PVRDvrData         *m_dvr;

extern PVRSchedulerThread *p_Scheduler;
extern PVRRecorderThread  *p_Recorder;
extern PVRReaderThread    *p_Reader;

extern void CloseThreads(void)
{
  if (p_Reader)
  {
    p_Reader->StopThread();
    SAFE_DELETE(p_Reader);
    XBMC->Log(LOG_NOTICE, "Reader thread deleted");
  }
 
  m_dvr->SetLock();

  PVRDvrTimer currTimer;

  map<int, PVRDvrTimer> Timers = m_dvr->GetTimerData();
  for (map<int, PVRDvrTimer>::iterator tmr=Timers.begin(); tmr != Timers.end(); ++tmr)
  {
    currTimer = tmr->second;

    if (currTimer.Status != PVR_STREAM_NO_STREAM && currTimer.Status != PVR_STREAM_STOPPED)
    {
      XBMC->Log(LOG_NOTICE, "Recording is still active, trying to stop: %s", currTimer.Timer.strTitle);
      currTimer.Status = PVR_STREAM_IS_STOPPING;
      m_dvr->UpdateTimer(currTimer);

      while (currTimer.Status != PVR_STREAM_NO_STREAM && currTimer.Status != PVR_STREAM_STOPPED)
      {    
        m_dvr->GetTimer(currTimer.Timer, currTimer);
        XBMC->Log(LOG_NOTICE, "Stopping recording %s", currTimer.Timer.strTitle);
      }
    }
  }

  if (p_Scheduler)
  {
    p_Scheduler->StopThread();
    SAFE_DELETE(p_Scheduler);
    XBMC->Log(LOG_NOTICE, "Scheduler thread deleted");
  }
 
  if (p_Recorder)
  {
    p_Recorder->StopThread();
    SAFE_DELETE(p_Recorder);
    XBMC->Log(LOG_NOTICE, "Recorder thread deleted");
  }

  m_dvr->SetUnlock();
}

extern string BuildSMBPath(string netPath)
{
  if (netPath.find("smb://") != 0)
    return netPath;

  string smb = "smb://";
  string delim = "/";
  auto start = smb.length();
  auto end = netPath.find(delim, start);
  auto nfound = 1;

  while (end != std::string::npos && nfound < 3)
  {
    // create SMB url for ffmpeg
    if (nfound == 1)
      smb.append(netPath.substr(start, end - start)+".local/");
    else
      smb.append(netPath.substr(start, end - start)+"/");

    // count tokens
    ++nfound;

    // split by tokens
    if (nfound < 3)
    {
      start = end + delim.length();
      end = netPath.find(delim, start);
    }
  }

  if (netPath.length() > end)
    smb.append(netPath.substr(end+1));

  return smb;
}

extern int strtoint(string s)
{
  int tmp = 0;
  unsigned int i = 0;
  bool m = false;

  if(s[0] == '-')
  {
    m = true;
    i++;
  }
  
  for(; i < s.size(); i++)
    tmp = 10 * tmp + s[i] - 48;
   
  return m ? -tmp : tmp;
}

extern string inttostr(int i)
{
  string tmp;
  tmp = to_string(i);
  return tmp;
}