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

#include <time.h>
#include "PVRSchedulerThread.h"
#include "PVRPlayList.h"
#include "PVRUtils.h"
#include "p8-platform/util/StringUtils.h"

using namespace ADDON;

extern PVRDvrData         *m_dvr;

extern std::string         s_system;

extern PVRSchedulerThread *p_Scheduler;
extern int64_t             i_timersFileSize;
extern bool                p_getTimersTransferFinished;
bool                       s_triggerTimerUpdate;

extern PVRRecorderThread  *p_Recorder;
extern int64_t             i_recordingsFileSize;
extern bool                p_getRecordingTransferFinished;
extern bool                s_triggerRecordingUpdate;

PVRSchedulerThread::PVRSchedulerThread(void)
{
  XBMC->Log(LOG_NOTICE, "Creating scheduler thread");

  b_interval           = 10;
  b_isWorking          = false;
  b_stop               = false;
  b_lastCheck          = time(NULL)-b_interval;
  CreateThread();
  s_triggerTimerUpdate = false;
}

PVRSchedulerThread::~PVRSchedulerThread(void)
{
  b_stop = true;

  XBMC->Log(LOG_NOTICE, "Closing scheduler thread");

  while (b_isWorking == true)
    XBMC->Log(LOG_NOTICE, "Waiting for close scheduler thread");
}

void PVRSchedulerThread::StopThread(bool bWait /*= true*/)
{
  b_stop = true;

  while (b_isWorking == true)
    XBMC->Log(LOG_DEBUG, "Stopping scheduler thread");

  CThread::StopThread(bWait);
}

void *PVRSchedulerThread::Process(void)
{
  XBMC->Log(LOG_NOTICE, "Starting scheduler thread");
  b_isWorking = true;

  while (!b_stop)
  {
    sleep(1);    
    time_t now = time(NULL);
    
    if (now >= b_lastCheck+b_interval) 
    {
      if (m_dvr->GetTimersAmount() > 0)
      {
      //XBMC->Log(LOG_NOTICE, "Checking for scheduled jobs");
        PVRIptvChannel currChannel;
        PVRDvrTimer currTimer;
        bool jobsUpdated = false;
        m_dvr->SetLock();
           
        std::map<int, PVRDvrTimer> Timers = m_dvr->GetTimerData();

        for (std::map<int, PVRDvrTimer>::iterator tmr=Timers.begin(); tmr != Timers.end(); ++tmr)
        {
          currTimer = tmr->second;

          if (currTimer.Timer.state == PVR_TIMER_STATE_CANCELLED) 
          {
            XBMC->Log(LOG_NOTICE, "Try to delete timer %s", currTimer.Timer.strTitle);
            
            if (m_dvr->DeleteTimer(currTimer, true))
              XBMC->Log(LOG_NOTICE, "Successfully deleted.");  
          }
          else if (currTimer.Status == PVR_STREAM_STOPPED)
          {
            if (currTimer.Timer.iTimerType != PVR_TIMER_TYPE_NONE) 
            {
              XBMC->Log(LOG_NOTICE, "Try to reschedule timer %s", currTimer.Timer.strTitle);
            
              if (!m_dvr->RescheduleTimer(currTimer)) 
              {
                XBMC->Log(LOG_NOTICE, "Try to delete timer %s", currTimer.Timer.strTitle);

                if (m_dvr->DeleteTimer(currTimer, true))
                {
                  XBMC->Log(LOG_NOTICE, "Successfully deleted.");

                  if (p_Recorder)
                  {
                    SAFE_DELETE(p_Recorder);
                    XBMC->Log(LOG_NOTICE, "Recorder thread deleted"); 
                  }
                }
              }
            }
            else 
            {
              if (m_dvr->DeleteTimer(currTimer, true))
                XBMC->Log(LOG_NOTICE, "Successfully deleted.");  
            }

      
            s_triggerTimerUpdate = true;
          }
          else if (currTimer.Timer.endTime < time(NULL) && !(currTimer.Timer.state == PVR_TIMER_STATE_RECORDING))
          {
            if (currTimer.Timer.iTimerType != PVR_TIMER_TYPE_NONE) 
            {
              XBMC->Log(LOG_NOTICE, "Try to reschedule timer %s", currTimer.Timer.strTitle);
          
              if (!m_dvr->RescheduleTimer(currTimer)) 
              {
                XBMC->Log(LOG_NOTICE, "Try to delete timer %s", currTimer.Timer.strTitle);

                if (m_dvr->DeleteTimer(currTimer, true))
                {
                  XBMC->Log(LOG_NOTICE, "Successfully deleted.");

                  if (p_Recorder)
                  {
                    SAFE_DELETE(p_Recorder);
                    XBMC->Log(LOG_NOTICE, "Recorder thread deleted"); 
                  }
                }
              }
            }
            else 
            {
              XBMC->Log(LOG_NOTICE, "Try to delete timer %s", currTimer.Timer.strTitle);

              if (m_dvr->DeleteTimer(currTimer, true))
                XBMC->Log(LOG_NOTICE, "Successfully deleted.");
            }
        
            s_triggerTimerUpdate = true;
          }
          else if ((currTimer.Timer.startTime-10) <= time(NULL) && currTimer.Timer.state == PVR_TIMER_STATE_SCHEDULED && currTimer.Timer.firstDay <= time(NULL))
          {
            // start new Recording
            XBMC->Log(LOG_NOTICE, "Try to start recording %s", currTimer.Timer.strTitle);
            StartRecording(currTimer);
            s_triggerTimerUpdate = true;
          }
          else if (currTimer.Timer.endTime <= time(NULL) && (currTimer.Timer.state == PVR_TIMER_STATE_RECORDING))
          {
            // stop Recording
            XBMC->Log(LOG_NOTICE, "Try to stop recording %s", currTimer.Timer.strTitle);
            StopRecording(currTimer);
            s_triggerTimerUpdate = true;
          }
        }
              
        m_dvr->SetUnlock();

        if (p_getTimersTransferFinished == true && s_triggerTimerUpdate == true)
        {
          s_triggerTimerUpdate = false;
    
          try 
          {
            p_getTimersTransferFinished = false;
            PVR->TriggerTimerUpdate();
          }
          catch( std::exception const & e ) 
          {
            // closing Kodi, TriggerTimerUpdate is not available
          }
        }

        if (p_getRecordingTransferFinished == true && s_triggerRecordingUpdate == true)
        {
          s_triggerRecordingUpdate = false;

          try 
          {
            p_getRecordingTransferFinished = false;
            PVR->TriggerRecordingUpdate();
          }
          catch( std::exception const & e ) 
          {
            // closing Kodi, TriggerRecordingUpdate is not available
          }
        }
      }    

      s_jobFile = g_strRecPath+TIMERS_FILE_NAME;
      v_fileHandle = XBMC->OpenFile(s_jobFile.c_str(), 0);
      if (v_fileHandle)
      {
        // read in updated timers file
        if (XBMC->GetFileLength(v_fileHandle) != i_timersFileSize)
        {
          if (m_dvr->ReLoadTimers())
          {
            XBMC->Log(LOG_DEBUG, "Reloaded timers successfully.");
            
            p_getTimersTransferFinished = false;
            PVR->TriggerTimerUpdate();
          }
        }

        XBMC->CloseFile(v_fileHandle);
      }

      s_jobFile = g_strRecPath+RECORDINGS_FILE_NAME;
      v_fileHandle = XBMC->OpenFile(s_jobFile.c_str(), 0);
  
      if (v_fileHandle)
      {
        // read in updated timers file
        if (XBMC->GetFileLength(v_fileHandle) != i_recordingsFileSize)
        {
          if (m_dvr->ReLoadRecordings())
          {
            XBMC->Log(LOG_DEBUG, "Reloaded recordings successfully.");

            p_getRecordingTransferFinished = false;
            PVR->TriggerRecordingUpdate();
          }
        }

        XBMC->CloseFile(v_fileHandle);
      }

      b_lastCheck=time(NULL);
    }
  }

  b_isWorking = false;
  return NULL;
}

bool PVRSchedulerThread::StartRecording(const PVRDvrTimer &myTimer)
{
  PVRDvrTimer currTimer;
  PVRIptvChannel currChannel;

  if (s_system == "LINUX")
  {
    if (m_dvr->GetTimer(myTimer.Timer, currTimer))
    { 
      if (m_dvr->GetChannel(myTimer.Timer, currChannel))
      {
        XBMC->Log(LOG_NOTICE, "Starting recording %s", myTimer.Timer.strTitle);
   
        currTimer.Status = PVR_STREAM_START_RECORDING;
        currTimer.Timer.state = PVR_TIMER_STATE_RECORDING;
        m_dvr->UpdateTimer(currTimer);

        p_Recorder = new PVRRecorderThread(currChannel, currTimer);
        currTimer.ThreadPtr = p_Recorder;
        return true;
      }
      else
      {
        // channel not found, update status
        currTimer.Status = PVR_STREAM_STOPPED;
        currTimer.Timer.state = PVR_TIMER_STATE_ERROR;
        m_dvr->UpdateTimer(currTimer);
      }
    }
  }
    
  return false;
}

bool PVRSchedulerThread::StopRecording(const PVRDvrTimer &myTimer)
{
  PVRDvrTimer currTimer;
  currTimer = myTimer;

  if (m_dvr->GetTimer(myTimer.Timer, currTimer))
  { 
    if (currTimer.Status != PVR_STREAM_STOPPED && currTimer.Timer.state != PVR_TIMER_STATE_NEW)
    {
      XBMC->Log(LOG_NOTICE, "Stopping recording %s", currTimer.Timer.strTitle);
      
      currTimer.Status = PVR_STREAM_IS_STOPPING;
      return (m_dvr->UpdateTimer(currTimer));
    }
  }
    
  return false;
}
