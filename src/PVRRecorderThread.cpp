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
#include <ctime>
#include <sstream>
#include <iomanip>

#include <time.h>
#include <unistd.h>
#include "PVRRecorderThread.h"
#include "PVRPlayList.h"
#include "PVRUtils.h"
#include "p8-platform/util/StringUtils.h"

using namespace ADDON;

extern std::string         g_strRecPath;
extern std::string         g_strFFMPEG;
extern std::string         g_strAVParams;
extern std::string         g_strFFPROBE;
extern std::string         g_strFileExt;
extern int                 g_iStrmTimeout;
extern std::string         g_strSmbPath;
extern std::string         g_strSmbMount;
extern std::string         g_strSmbUnmount;

extern PVRDvrData         *m_dvr;

extern PVRSchedulerThread *p_Scheduler;
extern int64_t             i_timersFileSize;
extern bool                p_getTimersTransferFinished;
extern bool                s_triggerTimerUpdate;

extern PVRRecorderThread  *p_Recorder;
extern int64_t             i_recordingsFileSize;
extern bool                p_getRecordingTransferFinished;
bool                       s_triggerRecordingUpdate;

PVRRecorderThread::PVRRecorderThread(PVRIptvChannel &currChannel, PVRDvrTimer &currTimer)
{
  XBMC->Log(LOG_NOTICE, "Creating recorder thread");

  t_currTimer              = currTimer;
  t_currChannel            = currChannel;
  s_triggerRecordingUpdate = false;
  CreateThread();
}

PVRRecorderThread::~PVRRecorderThread(void)
{
  PVRDvrTimer stopTimer;
  PVRIptvChannel currChannel;

  m_dvr->GetTimer(t_currTimer.Timer, stopTimer);        
  XBMC->Log(LOG_DEBUG, "Closing recorder thread %s", t_currTimer.Timer.strTitle);

  while (stopTimer.Status == PVR_STREAM_IS_RECORDING)
  {
    m_dvr->GetTimer(t_currTimer.Timer, stopTimer);
    XBMC->Log(LOG_DEBUG, "Waiting for close recorder thread %s", t_currTimer.Timer.strTitle);
  }
}

void PVRRecorderThread::StopThread(bool bWait /*= true*/)
{
  t_currTimer.Status = PVR_STREAM_IS_STOPPING;
  
  if (m_dvr->UpdateTimer(t_currTimer));
    XBMC->Log(LOG_DEBUG, "Stopping recorder thread %s", t_currTimer.Timer.strTitle);

  CThread::StopThread(bWait);
}

void PVRRecorderThread::CorrectDurationFLVFile(const std::string &videoFile, const double &duration)
{
  if (duration < 0)
  {
    XBMC->Log(LOG_NOTICE, "Duration correction failed");
    return;
  }

  // read 1024 first file bytes
  char buffer[1024];
  void *fileHandle;
  fileHandle = XBMC->OpenFile(videoFile.c_str(), 0);
  
  XBMC->ReadFile(fileHandle, buffer, 1024);
  XBMC->CloseFile(fileHandle);

  int loop_end = 1024-8-sizeof(double);
  int loop;
  int pos = -1;

  for (loop=0; loop<loop_end;loop++)
  {
    if (buffer[loop] == 'd' && buffer[loop+1] == 'u' && buffer[loop+2] == 'r' && buffer[loop+3] == 'a' && buffer[loop+4] == 't' && buffer[loop+5] == 'i' && buffer[loop+6] == 'o' && buffer[loop+7] == 'n')
      pos = loop;
  }

  // correct 1024 first file bytes
  if (pos>=0)
  {
    pos = pos+9;
    union
    {
      unsigned char dc[8];
      double dd;
    } d;
  
    d.dd = duration;
    long one = 1;
  
    // is isBigEndian?
    if(!(*((char *)(&one))))
    {
      buffer[pos+0] = d.dc[0]; buffer[pos+1] = d.dc[1]; buffer[pos+2] = d.dc[2]; buffer[pos+3] = d.dc[3]; buffer[pos+4] = d.dc[4]; buffer[pos+5] = d.dc[5]; buffer[pos+6] = d.dc[6]; buffer[pos+7] = d.dc[7];
    }
    else
    {
      buffer[pos+0] = d.dc[7]; buffer[pos+1] = d.dc[6]; buffer[pos+2] = d.dc[5]; buffer[pos+3] = d.dc[4]; buffer[pos+4] = d.dc[3]; buffer[pos+5] = d.dc[2]; buffer[pos+6] = d.dc[1]; buffer[pos+7] = d.dc[0];
    }
        
    fileHandle = XBMC->OpenFileForWrite(videoFile.c_str(), 0);
  
    XBMC->SeekFile(fileHandle, 0, ios::beg);
  
    int size = XBMC->WriteFile(fileHandle, buffer, 1024);
  
    if (size>0) 
    {
      XBMC->Log(LOG_NOTICE, "Duration corrected");
    }
    
    XBMC->CloseFile(fileHandle);  
    return;
  }
    
  XBMC->Log(LOG_NOTICE, "Duration correction failed");
}

void *PVRRecorderThread::Process(void)
{  
  PVRDvrTimer nowTimer; 

  if (!m_dvr->GetTimer(t_currTimer.Timer, nowTimer))
  {
    XBMC->Log(LOG_ERROR, "Failed to get current timer state.");
    return NULL;       
  }

  if (nowTimer.Status == PVR_STREAM_START_RECORDING)
  {
    nowTimer.Status = PVR_STREAM_IS_RECORDING;

    if (!m_dvr->UpdateTimer(nowTimer))
    {
      XBMC->Log(LOG_ERROR, "Failed to set timer state.");
      return NULL;    
    }
  }
    
  struct tm *current;
  time_t now;

  time(&now);
  current = localtime(&now);
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
    
  XBMC->Log(LOG_NOTICE, "Try to open stream %s", t_currChannel.strStreamURL.c_str());
    
  // get play list
  vector<std::string> vstrList;
  PVRPlayList* m_Playlist = new PVRPlayList();
  std::string strStreamUrl = t_currChannel.strStreamURL;
  m_Playlist->GetPlaylist(strStreamUrl, vstrList);
  SAFE_DELETE(m_Playlist);
    
  std::string filename = nowTimer.Timer.strTitle;
  filename = filename+strDate+"."+g_strFileExt;
    
  std::string illegalChars = "\\/:?\"<>|*'";
  std::string::iterator it ( filename.begin() );
  for (it = filename.begin(); it < filename.end(); ++it)
  {
    bool found = illegalChars.find(*it) != std::string::npos;

    if(found)
    {
      *it = ' ';
    }
  }
    
  // create kodi file path
  std::string videoFile = g_strRecPath + filename;

  // create newtwork file path accounting for SAMBA
  std::string filePath;

  if (g_strSmbPath.length() == 0)
    filePath = g_strRecPath + filename;
  else 
    filePath = g_strSmbPath + filename;
    
  XBMC->Log(LOG_NOTICE, "File to write: %s ", filePath.c_str());
  
  std::string strParams;
  std::string strCommand;
    
  double duration = nowTimer.Timer.endTime-nowTimer.Timer.startTime;
  t_duration = duration;
  double length = 0;
    
  int rtmpStream = 0;
  if(strStreamUrl.substr(0, 7) == "rtmp://"   ||
     strStreamUrl.substr(0, 8) == "rtmpt://"  ||
     strStreamUrl.substr(0, 8) == "rtmpe://"  ||
     strStreamUrl.substr(0, 9) == "rtmpte://" ||
     strStreamUrl.substr(0, 8) == "rtmps://"    )
  {
    rtmpStream = 1;
  }
    
  vector<std::string> stremaUrlVect = StringUtils::Split(strStreamUrl, " ");
  
  if (stremaUrlVect.size()>0)
  {
    strParams = stremaUrlVect[0];
  
    for (unsigned int i=1; i<stremaUrlVect.size(); i++) 
    {
      std::string line = StringUtils::Trim(stremaUrlVect[i]);
  
      if (line.length()>0)
      {
        vector<std::string> lineVect = StringUtils::Split (line, "=");
    
        if (lineVect[0] == "live") 
        {
          strParams = strParams + " --live";
        }
        else 
        {
          if (rtmpStream == 1) 
          {
            if (lineVect.size()>1)
            {
              strParams = strParams + " --"+lineVect[0]+"="+lineVect[1];
            }
            else 
            {
              strParams = strParams + " --"+line;
            }
          }
          else 
          {
            strParams = strParams + " --"+line;
          }
        }
      }
    }
  }

  // get rec info
  std::string recordingId = to_string(nowTimer.Timer.iClientIndex);
  std::string title = nowTimer.Timer.strTitle;
  std::string plotOutline = nowTimer.strPlotOutline.c_str();
  std::string plot = nowTimer.strPlot.c_str();
  std::string channel = nowTimer.strChannelName;
  std::string thumbnailPath = nowTimer.strIconPath.c_str();
  time_t recTime = now;
//std::string duration = NULL (will read back from file importing recording to Kodi) /* not avialable in kodi 17.6 yet */
  std::string genre = to_string(nowTimer.Timer.iGenreType);
  std::string genreSubtype = to_string(nowTimer.Timer.iGenreSubType);
  std::string genreDesc = nowTimer.strGenre.c_str();
  std::string channelId = to_string(nowTimer.Timer.iClientChannelUid);

  // cap plot text to 1024, max of struct
  if (plot.size() >= 1020)
    plot = plot.substr(0, 1020)+"....";

  // get import codes  
  strParams =  " -i \""+strParams+"\" "+g_strAVParams+" -f "+g_strFileExt+" \""+filePath+"\"";

  if (g_strFFMPEG.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Path to ffmpeg binary is not set. Please change addon configuration.");
  
    nowTimer.Status = PVR_STREAM_STOPPED;
    nowTimer.Timer.state= PVR_TIMER_STATE_ERROR;

    if (m_dvr->UpdateTimer(nowTimer));
      s_triggerTimerUpdate = true;
              
    return NULL;
  }
  
  if (g_strAVParams.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Recompression params for ffmpeg are not set. Please change addon configuration.");

    nowTimer.Status = PVR_STREAM_STOPPED;
    nowTimer.Timer.state= PVR_TIMER_STATE_ERROR;

    if (m_dvr->UpdateTimer(nowTimer));
      s_triggerTimerUpdate = true;
            
    return NULL;
  }

  if (g_strFFPROBE.length() == 0)
  {
    XBMC->Log(LOG_ERROR, "Path to ffprobe binary is not set. Please change addon configuration.");
  
    nowTimer.Status = PVR_STREAM_STOPPED;
    nowTimer.Timer.state= PVR_TIMER_STATE_ERROR;

    if (m_dvr->UpdateTimer(nowTimer));
      s_triggerTimerUpdate = true;
              
    return NULL;
  }
  
  std::string strCommandLog = g_strFFMPEG+strParams;
  XBMC->Log(LOG_NOTICE, "Starting ffmpeg: %s", strCommandLog.c_str());
    
  // POSIX
  e_Stream.set_binary_mode(exec_stream_t::s_out);
  e_Stream.set_wait_timeout(exec_stream_t::s_out, g_iStrmTimeout*1000);

  // mount SMB if enabled
  if (g_strSmbMount.length() > 0)
  {
    std::string strMountCmd = g_strSmbMount.substr(0, g_strSmbMount.find(" "));
    std::string strMountPar = g_strSmbMount.substr(g_strSmbMount.find(" "), g_strSmbMount.length());

    e_Stream.start(strMountCmd, strMountPar);
  }

  // run FFMPEG
  e_Stream.start(g_strFFMPEG, strParams);

  XBMC->Log(LOG_NOTICE, "Set stream timeout: %d", g_iStrmTimeout);

  // int bytes_read;
  void *fileHandle;
  std::string readBuffer;
  streamsize bytesRead;
  time_t lastRead = time(NULL);
  time_t recordStart = time(NULL);
  int64_t lastSize = 0;

  while(true)
  {
    std::string buff;
    try 
    {
      // read file
      if (XBMC->FileExists(videoFile.c_str(), false))
      {
        fileHandle = XBMC->OpenFile(videoFile.c_str(), true);

        if (XBMC->GetFileLength(fileHandle) > lastSize)
        {
          lastRead = time(NULL);
          lastSize = XBMC->GetFileLength(fileHandle);
        }
       
        XBMC->CloseFile(fileHandle); 
      }
    }
    catch( std::exception const & e ) 
    {
      //nothing to read
    }
  
    if (!m_dvr->GetTimer(t_currTimer.Timer, nowTimer))
      XBMC->Log(LOG_ERROR, "Failed to get current timer state.");       

    now = time(NULL);
    
    if (nowTimer.Timer.endTime<time(NULL) || nowTimer.Status == PVR_STREAM_IS_STOPPING || nowTimer.Status == PVR_STREAM_STOPPED || nowTimer.bIsDeleted == true)
    {
      // recording stopped, exit via subprocess q command
      e_Stream.in() << "q";
      e_Stream.close_in();

      // close exec stream
      if (!e_Stream.close())
      {
        XBMC->Log(LOG_NOTICE, "Recorder failed to close FFMPEG, killing process");
        e_Stream.kill();
      }
              
      XBMC->Log(LOG_NOTICE, "Recording stopped %s", nowTimer.Timer.strTitle);
                 
      // correct duration time & add to cache
      if (lastSize>0)
      {
        // correct duration
        strParams = " -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \""+filePath+"\"";

        e_Stream.start(g_strFFPROBE, strParams);

        getline(e_Stream.out(bytesRead), readBuffer, '\n' ).good();

        // close exec stream
        if (!e_Stream.close())
        {
          XBMC->Log(LOG_NOTICE, "Recorder failed to close FFPROBE, killing process");
          e_Stream.kill();
        }

        // correct duration
        double duration = now - recordStart;//atof(readBuffer.c_str());
        CorrectDurationFLVFile (videoFile, duration);
        m_dvr->SetLock();

        // create recording metadata and import to cache
        PVRDvrRecording currRecording;
        memset(&currRecording, 0, sizeof(PVRDvrRecording));

        PVR_STRCPY(currRecording.Recording.strRecordingId, recordingId.c_str());
        PVR_STRCPY(currRecording.Recording.strTitle, title.c_str());
        PVR_STRCPY(currRecording.Recording.strPlotOutline, plotOutline.c_str());
        PVR_STRCPY(currRecording.Recording.strPlot, plot.c_str());
        PVR_STRCPY(currRecording.Recording.strChannelName, channel.c_str());
        PVR_STRCPY(currRecording.Recording.strThumbnailPath, thumbnailPath.c_str());
        currRecording.Recording.recordingTime = recTime;
        currRecording.Recording.iDuration = duration;
        currRecording.Recording.iGenreType = atoi(genre.c_str());
        currRecording.Recording.iGenreSubType = atoi(genreSubtype.c_str());
      //PVR_STRCPY(currRecording.Recording.strGenreDescription, genreDesc.c_str());
        currRecording.Recording.iChannelUid = atoi(channelId.c_str());
        currRecording.Recording.channelType = PVR_RECORDING_CHANNEL_TYPE_TV;
        currRecording.strFileName = filename;
        currRecording.bIsDeleted = false;

        m_dvr->AddRecording(currRecording);
        m_dvr->SetUnlock();
      }
      else
      {
        XBMC->DeleteFile(videoFile.c_str());
      }

      nowTimer.Status = PVR_STREAM_STOPPED;
      nowTimer.Timer.state = PVR_TIMER_STATE_COMPLETED;
      m_dvr->UpdateTimer(nowTimer);
      s_triggerTimerUpdate = true;
      s_triggerRecordingUpdate = true;
      break;  
    }
    else if (now-lastRead>=g_iStrmTimeout)
    {
      // something wrong - data not growing, exit via subprocess q command
      e_Stream.in() << "q";
      e_Stream.close_in();

      // close exec stream
      if (!e_Stream.close())
      {
        XBMC->Log(LOG_NOTICE, "Recorder failed to close FFMPEG, killing process");
        e_Stream.kill();
      }         
         
      XBMC->Log(LOG_NOTICE, "Recording failed %s", nowTimer.Timer.strTitle);
                   
      // correct duration time & add to cache
      if (lastSize>0)
      {
        // correct duration
        strParams = " -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \""+filePath+"\"";

        e_Stream.start(g_strFFPROBE, strParams);

        getline(e_Stream.out(bytesRead), readBuffer, '\n' ).good();

        // close exec stream
        if (!e_Stream.close())
        {
          XBMC->Log(LOG_NOTICE, "Recorder failed to close FFPROBE, killing process");
          e_Stream.kill();
        }
          
        // correct duration
        double duration = atof(readBuffer.c_str());
        CorrectDurationFLVFile (videoFile, duration);
        m_dvr->SetLock();

        // create recording metadata and import to cache
        PVRDvrRecording currRecording;
        memset(&currRecording, 0, sizeof(PVRDvrRecording));

        PVR_STRCPY(currRecording.Recording.strRecordingId, recordingId.c_str());
        PVR_STRCPY(currRecording.Recording.strTitle, title.c_str());
        PVR_STRCPY(currRecording.Recording.strPlotOutline, plotOutline.c_str());
        PVR_STRCPY(currRecording.Recording.strPlot, plot.c_str());
        PVR_STRCPY(currRecording.Recording.strChannelName, channel.c_str());
        PVR_STRCPY(currRecording.Recording.strThumbnailPath, thumbnailPath.c_str());
        currRecording.Recording.recordingTime = recTime;
        currRecording.Recording.iDuration = duration;
        currRecording.Recording.iGenreType = atoi(genre.c_str());
        currRecording.Recording.iGenreSubType = atoi(genreSubtype.c_str());
      //PVR_STRCPY(currRecording.Recording.strGenreDescription, genreDesc.c_str());
        currRecording.Recording.iChannelUid = atoi(channelId.c_str());
        currRecording.Recording.channelType = PVR_RECORDING_CHANNEL_TYPE_TV;
        currRecording.strFileName = filename;
        currRecording.bIsDeleted = false;

        m_dvr->AddRecording(currRecording);
        m_dvr->SetUnlock();
      }
      else
      {
        XBMC->DeleteFile(videoFile.c_str());
      }

      nowTimer.Status = PVR_STREAM_STOPPED;
      nowTimer.Timer.state = PVR_TIMER_STATE_ERROR;
      m_dvr->UpdateTimer(nowTimer);
      s_triggerTimerUpdate = true;
      s_triggerRecordingUpdate = true;
      break;     
    }
  }

  // unmount SMB if enabled
  if (g_strSmbMount.length() > 0 && g_strSmbUnmount.length() > 0)
  {
    std::string strMountCmd = g_strSmbUnmount.substr(0, g_strSmbUnmount.find(" "));
    std::string strMountPar = g_strSmbUnmount.substr(g_strSmbUnmount.find(" "), g_strSmbUnmount.length());

    e_Stream.start(strMountCmd, strMountPar);
  }

  return NULL;  
}
