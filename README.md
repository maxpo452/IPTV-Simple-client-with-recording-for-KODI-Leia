# IPTV-Simple-client-with-recording-for-KODI-Leia
IPTV Simple client addon for KODI Leia. Based on IPTV Simple client and Gonzalo Vegas fork.

## Build instructions

### Linux

Make a directory where you'll keep both this repo and the xbmc repo first.

1. `git clone https://github.com/xbmc/xbmc.git`
2. `git checkout -b Leia remotes/origin/Leia`
2. `git clone https://github.com/maxpo452/IPTV-Simple-client-with-recording-for-KODI-Leia`
3. `cd IPTV-Simple-client-with-recording-for-KODI-Leia && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.iptvsimple -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make`

##### Requirements
1. FFMPEG binary

##### Tested with
1. Ubuntu 18.04 LTS and Kodi Leia


##### Credits
Credit to the following authors/projects who I borrowed some ideas and/or code from:
1. Gonzalo Vega who created the base of this project
