#!/bin/bash

export DISPLAY=:99
export QT_QUICK_BACKEND=software
export TS3CLIENT_NO_CRASHDUMP=1 
export QT_QPA_PLATFORM=xcb


pulseaudio --start --exit-idle-time=-1
export PULSE_SERVER=unix:/tmp/pulse.sock
Xorg :99 -noreset +extension GLX +extension RANDR +extension RENDER -logfile /tmp/xorg.log -config /etc/X11/xorg.conf &

sleep 10

cd /TeamSpeak3-Client-linux_amd64
./ts3client_linux_amd64 --no-sandbox --license_accepted=1 "ts3server://ts.109tech.com?port=9987&nickname=BOOTYCALL&channel=Generico"

