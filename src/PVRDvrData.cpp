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
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <mutex>

#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include "PVRIptvData.h"
#include "PVRDvrData.h"
#include "PVRReaderThread.h"
#include "PVRSchedulerThread.h"
#include "PVRRecorderThread.h"
#include "PVRUtils.h"
#include "p8-platform/util/StringUtils.h"

using namespace ADDON;

std::string         s_system;
std::mutex          p_mutex;

extern PVRIptvData *m_iptv;
extern std::string  g_strRecPath;
extern std::string  g_strFFMPEG;
extern std::string  g_strFileExt;

PVRReaderThread    *p_Reader;  

PVRSchedulerThread *p_Scheduler;
int64_t             i_timersFileSize;
bool                p_getTimersTransferFinished;
extern bool         s_triggerTimerUpdate;

PVRRecorderThread  *p_Recorder;
int64_t             i_recordingsFileSize;
bool                p_getRecordingTransferFinished;
extern bool         s_triggerRecordingUpdate;

PVRDvrData::PVRDvrData()
{
  #ifdef __ANDROID__
    s_system = "ANDROID";
  #else
    s_system = "LINUX";
  #endif

  if (s_system == "LINUX")
     XBMC->Log(LOG_NOTICE, "Linux system detected.");

  m_timers.clear();
  m_recordings.clear();

  if (LoadTimers())
    XBMC->QueueNotification(QUEUE_INFO, "%d timers loaded.", m_timers.size());

  if (LoadRecordings())
    XBMC->QueueNotification(QUEUE_INFO, "%d recordings loaded.", m_recordings.size());

  p_getTimersTransferFinished    = true;
  p_getRecordingTransferFinished = true;

  CreateThread();
}

void *PVRDvrData::Process(void)
{
  p_Scheduler = new PVRSchedulerThread();

  return NULL;
}

PVRDvrData::~PVRDvrData(void)
{
  m_timers.clear();
  m_recordings.clear();
}

bool PVRDvrData::LoadTimers(void)
{
  std::string jobFile = g_strRecPath+TIMERS_FILE_NAME;
  std::string strContent;
  void* fileHandle = XBMC->OpenFile(jobFile.c_str(), 0);
  
  if (fileHandle)
  {
    i_timersFileSize = XBMC->GetFileLength(fileHandle);

    char buffer[1024];
    while (int bytesRead = XBMC->ReadFile(fileHandle, buffer, 1024))
    {
      strContent.append(buffer, bytesRead);
    }
  
    XBMC->CloseFile(fileHandle);
    std::vector<std::string>ContVect = StringUtils::Split (strContent, "\n");
    std::vector<std::string>::iterator it ( ContVect.begin() );
    std::string buffStr;
    std::string strContentOut = "";

    for (it=ContVect.begin(); it != ContVect.end(); it++)
    {
      buffStr = *it;
      buffStr = StringUtils::Trim(buffStr);
      PVRDvrTimer jobEntry;
  
      if (ParseTimerString(buffStr, jobEntry))
      {
        jobEntry.bIsDeleted = false;

        if (jobEntry.Timer.state == PVR_TIMER_STATE_NEW || jobEntry.Timer.state == PVR_TIMER_STATE_SCHEDULED || jobEntry.Timer.state == PVR_TIMER_STATE_RECORDING)
        {
          if (jobEntry.Timer.startTime > time(NULL) || jobEntry.Timer.iTimerType != PVR_TIMER_TYPE_NONE || jobEntry.Timer.endTime > time(NULL))
          {
            m_timers[jobEntry.Timer.iClientIndex] = jobEntry; 
          }
        }
      }
    }

    XBMC->Log(LOG_NOTICE, "Loaded %d timers.", m_timers.size());  
    return true;
  }
  else
  {
    i_timersFileSize = 0;
    StoreTimerData();
    XBMC->Log(LOG_NOTICE, "No timers cache found, created new one.");
  }

  return false;
}

bool PVRDvrData::ReLoadTimers(void)
{
  for (std::map<int, PVRDvrTimer>::iterator it=m_timers.begin(); it != m_timers.end(); ++it)
  {
    // assume timers are deleted
    it->second.bIsDeleted = true;
  }

  return (LoadTimers());
}

bool PVRDvrData::LoadRecordings(void)
{
  std::string jobFile = g_strRecPath+RECORDINGS_FILE_NAME;
  std::string strContent;
  void* fileHandle = XBMC->OpenFile(jobFile.c_str(), 0);
  
  if (fileHandle)
  {
    i_recordingsFileSize = XBMC->GetFileLength(fileHandle);

    char buffer[1024];
    while (int bytesRead = XBMC->ReadFile(fileHandle, buffer, 1024))
    {
      strContent.append(buffer, bytesRead);
    }
  
    XBMC->CloseFile(fileHandle);
    std::vector<std::string>ContVect = StringUtils::Split (strContent, "\n");
    std::vector<std::string>::iterator it ( ContVect.begin() );
    std::string buffStr;
    std::string strContentOut = "";

    for (it=ContVect.begin(); it != ContVect.end(); it++)
    {
      buffStr = *it;
      buffStr = StringUtils::Trim(buffStr);
      PVRDvrRecording recEntry;
  
      if (ParseRecordingString(buffStr, recEntry))
      {
        recEntry.bIsDeleted = false;

        std::string strFilePath = g_strRecPath+recEntry.strFileName;

        if (XBMC->FileExists(strFilePath.c_str(), false))
          m_recordings[strtoint(recEntry.Recording.strRecordingId)] = recEntry; 
        else
          XBMC->Log(LOG_NOTICE, "Could not find recording file %s", strFilePath.c_str());
      }
    }
  
    XBMC->Log(LOG_NOTICE, "Loaded %d recordings.", m_recordings.size());
    return true;
  }
  else
  {
    i_recordingsFileSize = 0;
    StoreRecordingData();
    XBMC->Log(LOG_NOTICE, "No recordings cache found, created new one.");
  }

  return false;
}

bool PVRDvrData::ReLoadRecordings(void)
{
  for (std::map<int, PVRDvrRecording>::iterator it=m_recordings.begin(); it != m_recordings.end(); ++it)
  {
    // assume recordings are deleted
    it->second.bIsDeleted = true;
  }

  return (LoadRecordings());
}

bool PVRDvrData::StoreTimerData(void)
{
  std::string strContent;
  
  for (std::map<int, PVRDvrTimer>::iterator it=m_timers.begin(); it != m_timers.end(); ++it)
  {
    if (!it->second.bIsDeleted)
      strContent.append(GetTimerString(it->second));
  }

  i_timersFileSize = strContent.length();

  std::string jobFile = g_strRecPath+TIMERS_FILE_NAME;
  XBMC->DeleteFile(jobFile.c_str());
  void* writeFileHandle = XBMC->OpenFileForWrite(jobFile.c_str(), 1);
 
  if (writeFileHandle != NULL)
  {
    XBMC->WriteFile(writeFileHandle, strContent.c_str(), strContent.length());
    XBMC->CloseFile(writeFileHandle);
 
    return true;
  }

  return false;
}

bool PVRDvrData::StoreRecordingData (void)
{
  std::string strContent;
  
  for (std::map<int, PVRDvrRecording>::iterator it=m_recordings.begin(); it != m_recordings.end(); ++it)
  {
    if (!it->second.bIsDeleted)
      strContent.append(GetRecordingString(it->second));
  }

  i_recordingsFileSize = strContent.length();
 
  std::string jobFile = g_strRecPath+RECORDINGS_FILE_NAME;
  XBMC->DeleteFile(jobFile.c_str());
  void* writeFileHandle = XBMC->OpenFileForWrite(jobFile.c_str(), 1);
 
  if (writeFileHandle != NULL)
  {
    XBMC->WriteFile(writeFileHandle, strContent.c_str(), strContent.length());
    XBMC->CloseFile(writeFileHandle);
 
    return true;
  }
  
  return false;
}

PVR_ERROR PVRDvrData::GetTimers(ADDON_HANDLE handle)
{
  PVRIptvChannel currChannel;
  SetLock();

  for (std::map<int, PVRDvrTimer>::iterator tmr=m_timers.begin(); tmr != m_timers.end(); ++tmr)
  {
    if (GetChannel(tmr->second.Timer, currChannel))
    {
      if (!tmr->second.bIsDeleted)
        PVR->TransferTimerEntry(handle, &tmr->second.Timer);
    }
  }

  p_getTimersTransferFinished = true;
  SetUnlock();
  return PVR_ERROR_NO_ERROR;
}

int PVRDvrData::GetTimersAmount(void)
{
  return m_timers.size();
}

PVR_ERROR PVRDvrData::AddTimer(const PVR_TIMER &timer)
{
  if (g_strFileExt.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "No file extension. Please change addon configuration.");
    return PVR_ERROR_FAILED;
  }
        
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return PVR_ERROR_FAILED;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return PVR_ERROR_SERVER_ERROR; 
  }

  if (p_Recorder)
  {
    GUI->Dialog_OK_ShowAndGetInput("Information", "The backend currently does not support multi recording.");
    XBMC->Log(LOG_ERROR, "Attempted multi recording. Backend currently does not support this.");
    return PVR_ERROR_NOT_IMPLEMENTED;    
  }

  XBMC->Log(LOG_NOTICE, "Adding timer for %s", timer.strTitle);

  // set start time for Job
  time_t startTime = timer.startTime;

  if (timer.startTime >= timer.endTime)
    return PVR_ERROR_FAILED;

  // correct start time if needed
  if (timer.startTime == 0 || timer.startTime < time(NULL))
    startTime = time(NULL);

  // getChannel definition    
  PVRIptvChannel currChannel;
    
  if (!GetChannel(timer, currChannel))
    return PVR_ERROR_FAILED;

  // get EPG info
  PVR_CHANNEL channel;
  EPG_TAG recTag;

  channel.iUniqueId = timer.iClientChannelUid;
  bool foundEPG = false;

  if (m_iptv->GetEPGTagForChannel(recTag, channel, startTime, startTime) == PVR_ERROR_NO_ERROR)
  {
    if (!strcmp(timer.strTitle, recTag.strTitle))
    {
      XBMC->Log(LOG_NOTICE, "EPG for recording found, importing data");
      foundEPG = true;
    }
    else
    {
      XBMC->Log(LOG_NOTICE, "EPG data did not match recording");
      XBMC->Log(LOG_NOTICE, "Return title %s", recTag.strTitle);
    }
  }
  else
  {
    XBMC->Log(LOG_NOTICE, "Could not find EPG data for recording");
  }   
    
  // set end time for Job
  time_t endTime = timer.endTime;
    
  if (timer.startTime == 0)
  {     
    if (foundEPG == true)
    {
      if (recTag.endTime > time(NULL)+60)
        endTime = recTag.endTime;
    }
  }
    
  SetLock();
    
  // check, if job is aleready schduled for this time
  for (std::map<int, PVRDvrTimer>::iterator tmr=m_timers.begin(); tmr != m_timers.end(); ++tmr)
  {
    if (tmr->second.strChannelName == currChannel.strChannelName)
    {
      if ((tmr->second.Timer.startTime < startTime && startTime < tmr->second.Timer.endTime) ||
          (tmr->second.Timer.startTime < endTime   && endTime   < tmr->second.Timer.endTime) ||
          (startTime < tmr->second.Timer.startTime && tmr->second.Timer.endTime < endTime  )   ) 
      {
        // similar recording already scheduled
        if ((tmr->second.Timer.state == PVR_TIMER_STATE_SCHEDULED || (tmr->second.Timer.state == PVR_TIMER_STATE_RECORDING)) && (tmr->second.Status == PVR_STREAM_IS_RECORDING || tmr->second.Status == PVR_STREAM_START_RECORDING))
        {
          // send message to refresh timers
          SetUnlock();
          return PVR_ERROR_ALREADY_PRESENT;
        }
      }
    }
  }
    
  SetUnlock();
    
  // recalculate new job id
  int iJobId = timer.iClientIndex;
  if (iJobId <= 0)
    iJobId = time(NULL);
        
  // prepare new entry
  PVRDvrTimer currTimer;
  currTimer.Timer = timer;
  currTimer.Timer.iClientIndex = iJobId;
  currTimer.Timer.startTime = startTime;
  currTimer.Timer.endTime = endTime;
  currTimer.strChannelName = currChannel.strChannelName;

  // load EPG info
  if (foundEPG == true)
  {
    currTimer.strPlot = recTag.strPlot;
    currTimer.strPlotOutline = recTag.strPlotOutline;
    currTimer.strIconPath = recTag.strIconPath;
    currTimer.strGenre = recTag.strGenreDescription;
  }
  else
  {
    currTimer.strPlot = " ";
    currTimer.strPlotOutline = " ";
    currTimer.strIconPath = " ";
    currTimer.strGenre = " ";
  }

  // finalize entry
  currTimer.Status = PVR_STREAM_NO_STREAM;
  currTimer.Timer.state = PVR_TIMER_STATE_SCHEDULED;
  currTimer.bIsDeleted = false;
    
  // pass entry
  if (m_timers.find(currTimer.Timer.iClientIndex) == m_timers.end())
  { 
    m_timers[currTimer.Timer.iClientIndex] = currTimer; 

    if (StoreTimerData())
    {
      XBMC->Log(LOG_NOTICE, "Added timer entry %s", currTimer.Timer.strTitle);
      s_triggerTimerUpdate = true;
      return PVR_ERROR_NO_ERROR;
    }
  }

  return PVR_ERROR_FAILED;
}

PVR_ERROR PVRDvrData::DeleteTimer(const PVR_TIMER &timer, bool bForceDelete)
{    
  PVRDvrTimer currTimer;

  if (GetTimer(timer, currTimer))
  {
    if(!p_Scheduler->StopRecording(currTimer))
    {
      currTimer.Timer.state = PVR_TIMER_STATE_CANCELLED;
    }

    m_timers[currTimer.Timer.iClientIndex].bIsDeleted = true;

    if (StoreTimerData())
    {
      XBMC->Log(LOG_NOTICE, "Deleted timer entry %s", currTimer.Timer.strTitle);
      s_triggerTimerUpdate = true;
      return PVR_ERROR_NO_ERROR;
    }
  }

  return PVR_ERROR_FAILED;
}

PVR_ERROR PVRDvrData::UpdateTimer(const PVR_TIMER &timer)
{
  PVRDvrTimer currTimer;

  if (GetTimer(timer, currTimer))
  {    
    if (currTimer.Timer.state == PVR_TIMER_STATE_RECORDING && (currTimer.Status == PVR_STREAM_IS_RECORDING || currTimer.Status == PVR_STREAM_START_RECORDING || currTimer.Status == PVR_STREAM_IS_STOPPING))
    {
      // job is active
      if (timer.state == PVR_TIMER_STATE_CANCELLED)
      {
        // stop thread
        if (!p_Scheduler->StopRecording(currTimer))
        {
          return PVR_ERROR_FAILED;
        }
      }
    }

    // pass on new timer info
    currTimer.Timer = timer;
    m_timers[currTimer.Timer.iClientIndex] = currTimer; 
    
    if (StoreTimerData())
    {
      return PVR_ERROR_NO_ERROR;
    }
  }

  return PVR_ERROR_FAILED;
}

PVR_ERROR PVRDvrData::GetRecordings(ADDON_HANDLE handle)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return PVR_ERROR_FAILED;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return PVR_ERROR_SERVER_ERROR; 
  }

  SetLock();
    
  for (std::map<int, PVRDvrRecording>::iterator rec=m_recordings.begin(); rec != m_recordings.end(); ++rec)
  {
    // initialize PVR_REC (bug in call back function will not accpet &rec->second.Recording
    PVR_RECORDING recinfo;
    memset(&recinfo, 0, sizeof(PVR_RECORDING));
    bool importRec = false;

    PVR_STRCPY(recinfo.strRecordingId, rec->second.Recording.strRecordingId);
    PVR_STRCPY(recinfo.strTitle, rec->second.Recording.strTitle);
    PVR_STRCPY(recinfo.strPlotOutline, rec->second.Recording.strPlotOutline);
    PVR_STRCPY(recinfo.strPlot, rec->second.Recording.strPlot);
    PVR_STRCPY(recinfo.strChannelName, rec->second.Recording.strChannelName);
    PVR_STRCPY(recinfo.strThumbnailPath, rec->second.Recording.strThumbnailPath);
    recinfo.recordingTime = rec->second.Recording.recordingTime;
    recinfo.iDuration = rec->second.Recording.iDuration;
    recinfo.iGenreType = rec->second.Recording.iGenreType;
    recinfo.iGenreSubType = rec->second.Recording.iGenreSubType;
  //PVR_STRCPY(recinfo.strGenreDescription, rec->second.Recording.strGenreDescription);
    recinfo.iChannelUid = rec->second.Recording.iChannelUid;
    recinfo.channelType = rec->second.Recording.channelType;
    recinfo.iPlayCount = rec->second.Recording.iPlayCount;
    recinfo.iLastPlayedPosition = rec->second.Recording.iLastPlayedPosition;

    // check existance before transfer
    std::string strFilePath = g_strRecPath+rec->second.strFileName;

    if (XBMC->FileExists(strFilePath.c_str(), false))
    {
      PVR->TransferRecordingEntry(handle, &recinfo);
    } 
  }

  p_getRecordingTransferFinished = true;
  SetUnlock();
  return PVR_ERROR_NO_ERROR;
}

int PVRDvrData::GetRecordingsAmount(void)
{
  return m_recordings.size();
}

PVR_ERROR PVRDvrData::DeleteRecording(const PVR_RECORDING &recording)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return PVR_ERROR_FAILED;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return PVR_ERROR_SERVER_ERROR; 
  }

  PVRDvrRecording currRecording;

  if (GetRecording(recording, currRecording))
  {
    std::string delFile = g_strRecPath+currRecording.strFileName;

    XBMC->Log(LOG_NOTICE, "Attempting to delete %s", delFile.c_str());

    if (XBMC->DeleteFile(delFile.c_str()))
    {
      if (DeleteRecording(currRecording))
      {
        XBMC->Log(LOG_NOTICE, "Deleted file %s", delFile.c_str());
        s_triggerRecordingUpdate = true;
        return PVR_ERROR_NO_ERROR;
      }
    }

    XBMC->Log(LOG_ERROR, "Could not delete file, check write access.");
  }

  return PVR_ERROR_FAILED;
}

bool PVRDvrData::OpenRecordedStream(const PVR_RECORDING &recording)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return false;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return false;
  }

  // create file path
  struct tm *current;
  time_t recTime;

  recTime = recording.recordingTime;
  current = localtime(&recTime);
  std::string month = inttostr(current->tm_mon+1);
  if (current->tm_mon<10) month = "0"+month;
  std::string day = inttostr(current->tm_mday);
  if (current->tm_mday<10) day = "0"+day;
  std::string hour = inttostr(current->tm_hour);
  if (current->tm_hour<10) hour = "0"+hour;
  std::string min = inttostr(current->tm_min);
  if (current->tm_min<10) min = "0"+min;
  std::string sec = inttostr(current->tm_sec);
  if (current->tm_sec<10) sec = "0"+sec;
    
  std::string strDate = " ("+inttostr(current->tm_year+1900)+"-"+month+"-"+day+"T"+hour+"-"+min+"-"+sec+")";
  std::string recFile = g_strRecPath+recording.strTitle+strDate+"."+g_strFileExt;

  SetLock();

  // get duration in time_t
  time_t endTime;

  endTime = time(NULL) + recording.iDuration;

  // open file
  PVRDvrRecording currRecording;

  if (GetRecording(recording, currRecording))
  {
    std::string recFile = g_strRecPath+currRecording.strFileName;

    XBMC->Log(LOG_NOTICE, "Attempting to open %s", recFile.c_str());

    if (XBMC->FileExists(recFile.c_str(), false))
    {
      SetUnlock();
      p_Reader = new PVRReaderThread(strtoint(recording.strRecordingId), recFile.c_str(), endTime);
      return p_Reader->StartThread();
    }

    XBMC->Log(LOG_ERROR, "Could not open file, check read access.");
  }

  SetUnlock();

  return false;
}

void PVRDvrData::CloseRecordedStream(void)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
  }

  XBMC->Log(LOG_NOTICE, "Stopping playback of recording.");

  // stop reading
  if (p_Reader)
  {
    p_Reader->StopThread();
    SAFE_DELETE(p_Reader);
    XBMC->Log(LOG_NOTICE, "Reader thread deleted");
  }
} 

int PVRDvrData::ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return 0;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return 0;
  }

  // read recording
  if (p_Reader)
  {
    return p_Reader->ReadThread(pBuffer, iBufferSize);
  }

  return 0;
} 

long long PVRDvrData::SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return 0;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return 0;
  }

  // seek recording
  if (p_Reader)
  {
    return p_Reader->SeekThread(iPosition, iWhence);
  }

  return 0;
} 

long long PVRDvrData::GetReaderPosition(void)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return -1;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return -1;
  }

  // get position
  if (p_Reader)
  {
    return p_Reader->Position();
  }

  return -1;
} 

long long PVRDvrData::GetReaderLength(void)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return 0;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return 0;
  }

  // get length
  if (p_Reader)
  {
    return p_Reader->Position();
  }

  return 0;
} 

PVR_ERROR PVRDvrData::SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return PVR_ERROR_FAILED;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return PVR_ERROR_SERVER_ERROR; 
  }

  PVRDvrRecording currRecording;

  if (GetRecording(recording, currRecording))
  {
    std::string recFile = g_strRecPath+currRecording.strFileName;

    if (XBMC->FileExists(recFile.c_str(), false))
    {
      currRecording.Recording.iLastPlayedPosition = lastplayedposition;

      if (UpdateRecording(currRecording))
      {
        XBMC->QueueNotification(QUEUE_INFO, "Scrobbled successfully");
        XBMC->Log(LOG_NOTICE, "Playback srobbled successfully %s to %i secs", currRecording.Recording.strTitle, lastplayedposition);
        s_triggerRecordingUpdate = true;
        return PVR_ERROR_NO_ERROR;
      }      
    }

    XBMC->Log(LOG_ERROR, "Could not edit file, check write access.");
  }

  return PVR_ERROR_FAILED;
}

int PVRDvrData::GetRecordingLastPlayedPosition(const PVR_RECORDING &recording)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return -1;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return -1;
  }

  PVRDvrRecording currRecording;

  if (GetRecording(recording, currRecording))
  {
    std::string recFile = g_strRecPath+currRecording.strFileName;

    if (XBMC->FileExists(recFile.c_str(), false))
      return currRecording.Recording.iLastPlayedPosition;   

    XBMC->Log(LOG_ERROR, "Could not edit file, check write access.");
  }

  return -1;
}

PVR_ERROR PVRDvrData::SetRecordingPlayCount(const PVR_RECORDING &recording, int count)
{
  if (g_strRecPath.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Store folder is not set. Please change addon configuration.");
    return PVR_ERROR_FAILED;
  }

  if (!XBMC->DirectoryExists(g_strRecPath.c_str()))
  {
    XBMC->Log(LOG_ERROR, "Directory does not exist. Please check connection to network drives.");
    return PVR_ERROR_SERVER_ERROR; 
  }

  PVRDvrRecording currRecording;

  if (GetRecording(recording, currRecording))
  {
    std::string recFile = g_strRecPath+currRecording.strFileName;

    if (XBMC->FileExists(recFile.c_str(), false))
    {
      currRecording.Recording.iPlayCount = count;

      if (UpdateRecording(currRecording))
      {
        XBMC->Log(LOG_NOTICE, "Playback count added %s to %i times", currRecording.Recording.strTitle, count);
        s_triggerRecordingUpdate = true;
        return PVR_ERROR_NO_ERROR;
      }      
    }

    XBMC->Log(LOG_ERROR, "Could not edit file, check write access.");
  }

  return PVR_ERROR_FAILED;
}

std::map<int, PVRDvrTimer> PVRDvrData::GetTimerData(void)
{
  return m_timers;
}

bool PVRDvrData::GetChannel(const PVR_TIMER &timer, PVRIptvChannel &myChannel)
{
  PVR_CHANNEL channel;
  channel.iUniqueId = timer.iClientChannelUid;

  return m_iptv->GetChannel(channel, myChannel);
}

bool PVRDvrData::GetTimer(const PVR_TIMER &timer, PVRDvrTimer &myTimer)
{
  if (m_timers.find(timer.iClientIndex) == m_timers.end())
    return false;
    
  myTimer = m_timers[timer.iClientIndex];
  return true;
}

bool PVRDvrData::DeleteTimer(const PVRDvrTimer &myTimer, bool bForceDelete)
{    
  PVRDvrTimer currTimer;

  if (GetTimer(myTimer.Timer, currTimer))
  {
    if(!p_Scheduler->StopRecording(currTimer))
    {
      currTimer.Timer.state = PVR_TIMER_STATE_CANCELLED;
    }

    m_timers.erase(currTimer.Timer.iClientIndex);

    if (StoreTimerData())
    {
      XBMC->Log(LOG_NOTICE, "Deleted timer entry %s", currTimer.Timer.strTitle);;
      return true;
    }
  }

  return false;
}

bool PVRDvrData::UpdateTimer(const PVRDvrTimer &myTimer)
{
  PVRDvrTimer currTimer;

  if (GetTimer(myTimer.Timer, currTimer))
  {
    // status flow validation
    switch (currTimer.Status)
    {
      case PVR_STREAM_NO_STREAM :
        { if (myTimer.Status != PVR_STREAM_START_RECORDING && myTimer.Status != PVR_STREAM_NO_STREAM                                       ) return false; break; }
      case PVR_STREAM_START_RECORDING :
        { if (myTimer.Status != PVR_STREAM_IS_RECORDING && myTimer.Status != PVR_STREAM_START_RECORDING                                    ) return false; break; }
      case PVR_STREAM_IS_RECORDING :
        { if (myTimer.Status != PVR_STREAM_IS_STOPPING && myTimer.Status != PVR_STREAM_STOPPED && myTimer.Status != PVR_STREAM_IS_RECORDING) return false; break; }
      case PVR_STREAM_IS_STOPPING :
        { if (myTimer.Status != PVR_STREAM_STOPPED && myTimer.Status != PVR_STREAM_IS_STOPPING                                             ) return false; break; }
      case PVR_STREAM_STOPPED :
        { if (myTimer.Status != PVR_STREAM_STOPPED                                                                                         ) return false; break; }
    }
    
    // pass on new timer info
    currTimer = myTimer;
    m_timers[myTimer.Timer.iClientIndex] = currTimer; 

    if (StoreTimerData())
    {
      XBMC->Log(LOG_NOTICE, "Updated timer entry %s", currTimer.Timer.strTitle);
      return true; 
    }
  }
    
  return false;
}

bool PVRDvrData::RescheduleTimer(const PVRDvrTimer &myTimer)
{
  PVRDvrTimer currTimer = myTimer;
  
  if (GetTimer(myTimer.Timer, currTimer))
  {
    struct tm *startTime = localtime (&currTimer.Timer.startTime);
    int oneDayInt = 24*3600;

    bool reScheduled = false;

    // create DOW schedule std::map
    std::map<int, int> daysOfWeek;
    int i, k;
    
    k = 1;

    for (i=1; i<=7; i++) 
    {
      daysOfWeek[i] = currTimer.Timer.iWeekdays & k;
      daysOfWeek[i+7] = currTimer.Timer.iWeekdays & k;
      k = k<<1;
    }

    // calculate next DOW recording day
    int nextRecDays = 1;
        
    for (i=startTime->tm_wday+1; i<=14; i++)
    {
      if (daysOfWeek[i]>0) 
      {
        reScheduled = true;
        break;
      }
      
      nextRecDays++;
    }

    if (reScheduled == true) 
    {
      currTimer.Timer.startTime = currTimer.Timer.startTime+(oneDayInt*nextRecDays);
      currTimer.Timer.endTime = currTimer.Timer.endTime+(oneDayInt*nextRecDays);
      currTimer.Timer.firstDay = currTimer.Timer.startTime;
      currTimer.Timer.state = PVR_TIMER_STATE_SCHEDULED;
      currTimer.Status = PVR_STREAM_NO_STREAM;

      m_timers[myTimer.Timer.iClientIndex] = currTimer; 
      
      if (StoreTimerData())
      {
        XBMC->Log(LOG_NOTICE, "Rescheduled timer entry %s", myTimer.Timer.strTitle);
        return true;
      }
    }
  }
  
  return false;
}

std::map<int, PVRDvrRecording> PVRDvrData::GetRecordingData(void)
{
  return m_recordings;
}

bool PVRDvrData::GetRecording(const PVR_RECORDING &recording, PVRDvrRecording &myRecording)
{
  if (m_recordings.find(strtoint(recording.strRecordingId)) == m_recordings.end())
    return false;
    
  myRecording = m_recordings[strtoint(recording.strRecordingId)];
  return true;
}

bool PVRDvrData::AddRecording(const PVRDvrRecording &myRecording)
{ 
  PVRDvrRecording currRecording = myRecording;

  if (m_recordings.find(strtoint(myRecording.Recording.strRecordingId)) == m_recordings.end())
  { 
    m_recordings[strtoint(myRecording.Recording.strRecordingId)] = currRecording; 

    if (StoreRecordingData())
    {
      XBMC->Log(LOG_NOTICE, "Add recording entry %s", myRecording.strFileName.c_str());
      return true;
    }
  }

  return false;
}

bool PVRDvrData::DeleteRecording(const PVRDvrRecording &myRecording)
{
  PVRDvrRecording currRecording;
  
  if (GetRecording(myRecording.Recording, currRecording))
  {
    m_recordings.erase(strtoint(currRecording.Recording.strRecordingId));

    if (StoreRecordingData())
    {
      XBMC->Log(LOG_NOTICE, "Deleted recording entry %s", currRecording.strFileName.c_str());
      return true;
    }
  }

  return false;
}

bool PVRDvrData::UpdateRecording(const PVRDvrRecording &myRecording)
{
  PVRDvrRecording currRecording;

  if (GetRecording(myRecording.Recording, currRecording))
  {
    // status flow validation
    std::string strFilePath = g_strRecPath+currRecording.strFileName;

    if (!XBMC->FileExists(strFilePath.c_str(), false))
      return false;
    
    // pass on new rec info
    currRecording = myRecording;
    m_recordings[strtoint(myRecording.Recording.strRecordingId)] = currRecording; 

    if (StoreRecordingData())
    {
      XBMC->Log(LOG_NOTICE, "Updated recording entry %s", currRecording.strFileName.c_str());
      return true; 
    }
  }
    
  return false;
}

bool PVRDvrData::SetLock (void)
{
  p_mutex.lock();
  return true;
}

void PVRDvrData::SetUnlock (void)
{
  p_mutex.unlock();
}

std::string PVRDvrData::GetTimerString(const PVRDvrTimer &myTimer)
{
  std::string ChannelName = myTimer.strChannelName;
  StringUtils::Replace(ChannelName, "|", "\\|");
  StringUtils::Replace(ChannelName, "\n", "{n}");

  std::string strTitle = myTimer.Timer.strTitle;
  StringUtils::Replace(strTitle, "|", "\\|");
  StringUtils::Replace(strTitle, "\n", "{n}");
  
  std::string strEpgSearchString = myTimer.Timer.strEpgSearchString;
  StringUtils::Replace(strEpgSearchString, "|", "\\|");
  StringUtils::Replace(strEpgSearchString, "\n", "{n}");
  
  std::string strDirectory = myTimer.Timer.strDirectory;
  StringUtils::Replace(strDirectory, "|", "\\|");
  StringUtils::Replace(strDirectory, "\n", "{n}");
  
  std::string strSummary = myTimer.Timer.strSummary;
  StringUtils::Replace(strSummary, "|", "\\|");
  StringUtils::Replace(strSummary, "\n", "{n}");

  std::string Plot = myTimer.strPlot;
  StringUtils::Replace(Plot, "|", "\\|");
  StringUtils::Replace(Plot, "\n", "{n}");

  std::string PlotOutline = myTimer.strPlotOutline;
  StringUtils::Replace(PlotOutline, "|", "\\|");
  StringUtils::Replace(PlotOutline, "\n", "{n}");

  std::string IconPath = myTimer.strIconPath;
  StringUtils::Replace(IconPath, "|", "\\|");
  StringUtils::Replace(IconPath, "\n", "{n}");

  std::string Genre = myTimer.strGenre;
  StringUtils::Replace(Genre, "|", "\\|");
  StringUtils::Replace(Genre, "\n", "{n}");
  
  std::string Line = "\""+ChannelName;                                                                              //0
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iClientIndex);                                                         //1
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iParentClientIndex);                                                   //2
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iClientChannelUid);                                                    //3
  Line = Line+"\"|\""+inttostr(myTimer.Timer.startTime);                                                            //4
  Line = Line+"\"|\""+inttostr(myTimer.Timer.endTime);                                                              //5
  Line = Line+"\"|\""+inttostr(myTimer.Timer.state + (myTimer.Timer.state == PVR_TIMER_STATE_RECORDING ? 100 : 0)); //6
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iTimerType);                                                           //7
  Line = Line+"\"|\""+strTitle;                                                                                     //8
  Line = Line+"\"|\""+strEpgSearchString;                                                                           //9
  if (myTimer.Timer.bFullTextEpgSearch == true)
    Line = Line+"\"|\"1";
  else
    Line = Line+"\"|\"0";                                                                                           //10
  Line = Line+"\"|\""+strDirectory;                                                                                 //11
  Line = Line+"\"|\""+strSummary;                                                                                   //12
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iPriority);                                                            //13
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iLifetime);                                                            //14
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iRecordingGroup);                                                      //15
  Line = Line+"\"|\""+inttostr(myTimer.Timer.firstDay);                                                             //16
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iWeekdays);                                                            //17
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iPreventDuplicateEpisodes);                                            //18
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iEpgUid);                                                              //19
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iMarginStart);                                                         //20
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iMarginEnd);                                                           //21
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iGenreType);                                                           //22
  Line = Line+"\"|\""+inttostr(myTimer.Timer.iGenreSubType);                                                        //23
  Line = Line+"\"|\""+Plot;                                                                                         //24
  Line = Line+"\"|\""+PlotOutline;                                                                                  //25
  Line = Line+"\"|\""+IconPath;                                                                                     //26
  Line = Line+"\"|\""+Genre;                                                                                        //27
  Line = Line+"\"|\""+inttostr(myTimer.Status);                                                                     //28  
  Line = Line+"\"\n";
  
  return Line;
}

bool PVRDvrData::ParseTimerString(std::string buffStr, PVRDvrTimer &myTimer)
{
  buffStr = StringUtils::Trim(buffStr);
  if (buffStr.length() == 0)
    return false;

  //StringUtils::Trim " from start and EOL
  buffStr = buffStr.substr(1, buffStr.length()-2);
  if (buffStr.length() == 0)
    return false;

  std::vector<std::string> lineVect = StringUtils::Split(buffStr, "\"|\"");
  if (lineVect.size()<17)
    return false;

  myTimer.strChannelName                    = lineVect[0];                                                                         //0
  StringUtils::Replace(myTimer.strChannelName, "\\|", "|");
  StringUtils::Replace(myTimer.strChannelName, "{n}", "\n");
    
  myTimer.Timer.iClientIndex                = strtoint(lineVect[1]);                                                               //1
  myTimer.Timer.iParentClientIndex          = strtoint(lineVect[2]);                                                               //2
  myTimer.Timer.iClientChannelUid           = strtoint(lineVect[3]);                                                               //3
  myTimer.Timer.startTime                   = strtoint(lineVect[4]);                                                               //4
  myTimer.Timer.endTime                     = strtoint(lineVect[5]);                                                               //5
  myTimer.Timer.state                       = (PVR_TIMER_STATE) (strtoint(lineVect[6]) - (strtoint(lineVect[6]) > 100 ? 100 : 0)); //6
  myTimer.Timer.iTimerType                  = strtoint(lineVect[7]);                                                               //7
    
  std::string strTitle                               = lineVect[8];                                                                //8
  StringUtils::Replace(strTitle, "\\|", "|");
  StringUtils::Replace(strTitle, "{n}", "\n");
  strncpy(myTimer.Timer.strTitle, strTitle.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);
    
  std::string strEpgSearchString                     = lineVect[9];                                                                //9
  StringUtils::Replace(strEpgSearchString, "\\|", "|");
  StringUtils::Replace(strEpgSearchString, "{n}", "\n");
  strncpy(myTimer.Timer.strEpgSearchString, strEpgSearchString.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);
    
  myTimer.Timer.bFullTextEpgSearch                   = strtoint(lineVect[10]);                                                     //10
    
  std::string strDirectory                           = lineVect[11];                                                               //11
  StringUtils::Replace(strDirectory, "\\|", "|");
  StringUtils::Replace(strDirectory, "{n}", "\n");
  strncpy(myTimer.Timer.strDirectory, strDirectory.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);
    
  std::string strSummary                              = lineVect[12];                                                              //12
  StringUtils::Replace(strSummary, "\\|", "|");
  StringUtils::Replace(strSummary, "{n}", "\n");
  strncpy(myTimer.Timer.strSummary, strSummary.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);
    
  myTimer.Timer.iPriority                   = strtoint(lineVect[13]);                                                              //13
  myTimer.Timer.iLifetime                   = strtoint(lineVect[14]);                                                              //14
  myTimer.Timer.iRecordingGroup             = strtoint(lineVect[15]);                                                              //15
  myTimer.Timer.firstDay                    = strtoint(lineVect[16]);                                                              //16
  myTimer.Timer.iWeekdays                   = strtoint(lineVect[17]);                                                              //17
  myTimer.Timer.iPreventDuplicateEpisodes   = strtoint(lineVect[18]);                                                              //18
  myTimer.Timer.iEpgUid                     = strtoint(lineVect[19]);                                                              //19
  myTimer.Timer.iMarginStart                = strtoint(lineVect[20]);                                                              //20
  myTimer.Timer.iMarginEnd                  = strtoint(lineVect[21]);                                                              //21
  myTimer.Timer.iGenreType                  = strtoint(lineVect[22]);                                                              //22
  myTimer.Timer.iGenreSubType               = strtoint(lineVect[23]);                                                              //23

  myTimer.strPlot                           = lineVect[24];                                                                        //24
  StringUtils::Replace(myTimer.strPlot, "\\|", "|");
  StringUtils::Replace(myTimer.strPlot, "{n}", "\n");
 
  myTimer.strPlotOutline                    = lineVect[25];                                                                        //25
  StringUtils::Replace(myTimer.strPlotOutline, "\\|", "|");
  StringUtils::Replace(myTimer.strPlotOutline, "{n}", "\n");

  myTimer.strIconPath                       = lineVect[26];                                                                        //26
  StringUtils::Replace(myTimer.strIconPath, "\\|", "|");
  StringUtils::Replace(myTimer.strIconPath, "{n}", "\n");

  myTimer.strGenre                          = lineVect[27];                                                                        //27
  StringUtils::Replace(myTimer.strGenre, "\\|", "|");
  StringUtils::Replace(myTimer.strGenre, "{n}", "\n");

  myTimer.Status                            = (PVR_STREAM_STATUS) strtoint(lineVect[28]);                                          //28

  return true;
}

std::string PVRDvrData::GetRecordingString(const PVRDvrRecording &myRecording)
{
  std::string RecordingId = myRecording.Recording.strRecordingId;
  StringUtils::Replace(RecordingId, "|", "\\|");
  StringUtils::Replace(RecordingId, "\n", "{n}");

  std::string FileName = myRecording.strFileName;
  StringUtils::Replace(FileName, "|", "\\|");
  StringUtils::Replace(FileName, "\n", "{n}");

  std::string strTitle = myRecording.Recording.strTitle;
  StringUtils::Replace(strTitle, "|", "\\|");
  StringUtils::Replace(strTitle, "\n", "{n}");
  
  std::string strPlotOutline = myRecording.Recording.strPlotOutline;
  StringUtils::Replace(strPlotOutline, "|", "\\|");
  StringUtils::Replace(strPlotOutline, "\n", "{n}");

  std::string strPlot = myRecording.Recording.strPlot;
  StringUtils::Replace(strPlot, "|", "\\|");
  StringUtils::Replace(strPlot, "\n", "{n}");

  std::string strChannelName = myRecording.Recording.strChannelName;
  StringUtils::Replace(strChannelName, "|", "\\|");
  StringUtils::Replace(strChannelName, "\n", "{n}");

  std::string strThumbnailPath = myRecording.Recording.strThumbnailPath;
  StringUtils::Replace(strThumbnailPath, "|", "\\|");
  StringUtils::Replace(strThumbnailPath, "\n", "{n}");

  std::string strPlayCount = "0";
  if (myRecording.Recording.iPlayCount > 0)
    strPlayCount = inttostr(myRecording.Recording.iPlayCount);

  std::string strLastPlayedPosition = "0";
  if (myRecording.Recording.iLastPlayedPosition > 0)
    strLastPlayedPosition = inttostr(myRecording.Recording.iLastPlayedPosition);

  std::string Line = "\""+RecordingId;                                           //0
  Line = Line+"\"|\""+FileName;                                                  //1
  Line = Line+"\"|\""+strTitle;                                                  //2
  Line = Line+"\"|\""+strPlotOutline;                                            //3
  Line = Line+"\"|\""+strPlot;                                                   //4
  Line = Line+"\"|\""+strChannelName;                                            //5
  Line = Line+"\"|\""+strThumbnailPath;                                          //6
  Line = Line+"\"|\""+inttostr(myRecording.Recording.recordingTime);             //7
  Line = Line+"\"|\""+inttostr(myRecording.Recording.iDuration);                 //8
  Line = Line+"\"|\""+inttostr(myRecording.Recording.iGenreType);                //9
  Line = Line+"\"|\""+inttostr(myRecording.Recording.iGenreSubType);             //10
  Line = Line+"\"|\""+inttostr(myRecording.Recording.iChannelUid);               //11
  Line = Line+"\"|\""+inttostr(myRecording.Recording.channelType);               //12
  Line = Line+"\"|\""+strPlayCount;                                              //13
  Line = Line+"\"|\""+strLastPlayedPosition;                                     //14
  Line = Line+"\"\n";
  
  return Line;
}

bool PVRDvrData::ParseRecordingString(std::string buffStr, PVRDvrRecording &myRecording)
{
  buffStr = StringUtils::Trim(buffStr);
  if (buffStr.length() == 0)
    return false;

  //StringUtils::Trim " from start and EOL
  buffStr = buffStr.substr(1, buffStr.length()-2);
  if (buffStr.length() == 0)
    return false;

  std::vector<std::string> lineVect = StringUtils::Split(buffStr, "\"|\"");
  if (lineVect.size()<11)
    return false;

  std::string strRecordingId                       = lineVect[0];                                              //0
  StringUtils::Replace(strRecordingId, "\\|", "|");
  StringUtils::Replace(strRecordingId, "{n}", "\n");
  strncpy(myRecording.Recording.strRecordingId, strRecordingId.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);

  myRecording.strFileName                          = lineVect[1];                                              //1
  StringUtils::Replace(myRecording.strFileName, "\\|", "|");
  StringUtils::Replace(myRecording.strFileName, "{n}", "\n");

  std::string strTitle                             = lineVect[2];                                              //2
  StringUtils::Replace(strTitle, "\\|", "|");
  StringUtils::Replace(strTitle, "{n}", "\n");
  strncpy(myRecording.Recording.strTitle, strTitle.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);

  std::string strPlotOutline                       = lineVect[3];                                              //3
  StringUtils::Replace(strPlotOutline, "\\|", "|");
  StringUtils::Replace(strPlotOutline, "{n}", "\n");
  strncpy(myRecording.Recording.strPlotOutline, strPlotOutline.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);

  std::string strPlot                              = lineVect[4];                                              //4
  StringUtils::Replace(strPlot, "\\|", "|");
  StringUtils::Replace(strPlot, "{n}", "\n");
  strncpy(myRecording.Recording.strPlot, strPlot.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);

  std::string strChannelName                       = lineVect[5];                                              //5
  StringUtils::Replace(strChannelName, "\\|", "|");
  StringUtils::Replace(strChannelName, "{n}", "\n");
  strncpy(myRecording.Recording.strChannelName, strChannelName.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);

  std::string strThumbnailPath                     = lineVect[6];                                              //6
  StringUtils::Replace(strThumbnailPath, "\\|", "|");
  StringUtils::Replace(strThumbnailPath, "{n}", "\n");
  strncpy(myRecording.Recording.strThumbnailPath, strThumbnailPath.c_str(), PVR_ADDON_NAME_STRING_LENGTH - 1);    

  myRecording.Recording.recordingTime        = strtoint(lineVect[7]);                                          //7
  myRecording.Recording.iDuration            = strtoint(lineVect[8]);                                          //8
  myRecording.Recording.iGenreType           = strtoint(lineVect[9]);                                          //9
  myRecording.Recording.iGenreSubType        = strtoint(lineVect[10]);                                         //10
  myRecording.Recording.iChannelUid          = strtoint(lineVect[11]);                                         //11  
  myRecording.Recording.channelType          = (PVR_RECORDING_CHANNEL_TYPE) strtoint(lineVect[12]);            //12
  myRecording.Recording.iPlayCount           = strtoint(lineVect[13]);                                         //13
  myRecording.Recording.iLastPlayedPosition  = strtoint(lineVect[14]);                                         //14 
 
  return true;
}