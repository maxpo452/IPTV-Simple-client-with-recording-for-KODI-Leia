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

#include <string>
#include "p8-platform/util/util.h"

using namespace ADDON;

namespace ADDON
{
  struct PVRIptvEpgEntry
  {
    int         iBroadcastId;
    int         iChannelId;
    int         iGenreType;
    int         iGenreSubType;
    time_t      startTime;
    time_t      endTime;
    std::string strTitle;
    std::string strPlotOutline;
    std::string strPlot;
    std::string strIconPath;
    std::string strGenreString;
  };

  struct PVRIptvEpgChannel
  {
    std::string                  strId;
    std::string                  strName;
    std::string                  strIcon;
    std::vector<PVRIptvEpgEntry> epg;
  };

  struct PVRIptvChannel
  {
    bool        bRadio;
    int         iUniqueId;
    int         iChannelNumber;
    int         iEncryptionSystem;
    int         iTvgShift;
    std::string strChannelName;
    std::string strLogoPath;
    std::string strStreamURL;
    std::string strTvgId;
    std::string strTvgName;
    std::string strTvgLogo;
  };

  struct PVRIptvChannelGroup
  {
    bool             bRadio;
    int              iGroupId;
    std::string      strGroupName;
    std::vector<int> members;
  };

  struct PVRIptvEpgGenre
  {
    int         iGenreType;
    int         iGenreSubType;
    std::string strGenre;
  };

  typedef enum
  {
    PVR_STREAM_TYPE_RTMP              = 1,
    PVR_STREAM_TYPE_FILE              = 2
  } PVR_STREAM_TYPE;
    
  typedef enum
  {
    PVR_STREAM_NO_STREAM              = 0,
    PVR_STREAM_START_RECORDING        = 1,
    PVR_STREAM_IS_RECORDING           = 2,
    PVR_STREAM_IS_STOPPING            = 3,
    PVR_STREAM_STOPPED                = 4
  } PVR_STREAM_STATUS;
    
  struct PVRDvrTimer
  {
    PVR_TIMER         Timer;
    PVR_STREAM_STATUS Status;
    std::string       strChannelName;
    std::string       strPlot;
    std::string       strPlotOutline;
    std::string       strIconPath;
    std::string       strGenre;
    void*             ThreadPtr;
    bool              bIsDeleted; 
  };

  struct PVRDvrRecording
  {
    PVR_RECORDING Recording;
    std::string   strFileName;
    void*         ThreadPtr;
    bool          bIsDeleted; 
  };
}