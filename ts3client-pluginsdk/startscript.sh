#!/bin/bash

export XDG_RUNTIME_DIR=/tmp/runtime-root
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

pkill -f pulse
rm -rf /root/.config/pulse/*
rm -rf /tmp/pulse-*
# Start PulseAudio
#pulseaudio --system --exit-idle-time=-1 &
pulseaudio --system --exit-idle-time=-1 --disallow-exit --no-cpu-limit \
  --load="module-native-protocol-unix socket=/tmp/pulse.sock auth-anonymous=1" &
export PULSE_SERVER=unix:/tmp/pulse.sock

# Wait for PulseAudio socket
for i in {1..20}; do
  if [ -S /tmp/pulse.sock ]; then break; fi
  echo "Waiting for PulseAudio..."
  sleep 0.5
done

pkill -f Xorg
rm -f /tmp/.X99-lock
rm -rf /tmp/.X11-unix/X99
# Start Xorg in the background
Xorg :99 -ac -noreset +extension GLX +extension RANDR +extension RENDER -logfile /tmp/xorg.log -config /etc/X11/xorg.conf &

export DISPLAY=:99 

# Wait for X to be ready
for i in {1..20}; do
  if xdpyinfo -display :99 > /dev/null 2>&1; then break; fi
  echo "Waiting for Xorg..."
  sleep 0.5
done

echo "[INFO] pulse and x OK, waiting before starting ts..."
# Launch TS3
sleep 90 #wait some time to not DOS the server
mkdir -p /root/.ts3client
#   accept the license
# sqlite3 /root/.ts3client/settings.db "CREATE TABLE IF NOT EXISTS Misc (key TEXT PRIMARY KEY, value TEXT);"
# sqlite3 /root/.ts3client/settings.db "INSERT OR REPLACE INTO Misc (key, value) VALUES ('LicenseAccepted', '1');"
#   disable other dialogs
# cat ShowTS5Promotion=0 >> /root/.ts3client/settings.ini 
# cat ShowShowTS5Overview=0 >> /root/.ts3client/settings.ini 
# cat ShowDisableTS5Popup=1 >> /root/.ts3client/settings.ini 
# cat ShowShowUpgradeDialog=0 >> /root/.ts3client/settings.ini 
# cat ShowEnablePromotions=0 >> /root/.ts3client/settings.ini 
# cat NoAccountDialog=1 >> /root/.ts3client/settings.ini 
cd /TeamSpeak3-Client-linux_amd64/
# rm -rf plugins
echo "[INFO] starting ts..."
if ! [ -f /root/.ts3client/settings.db ]
then
  ./ts3client_linux_amd64 --no-sandbox "ts3server://ts.109tech.com?port=9987&nickname=BOOTYCALL&channel=Generico" &
  sleep 3
  pkill ts3client_linux
fi
echo "[INFO] checking if this is the first time the docker volume is created"
if [ $(strings /root/.ts3client/settings.db | grep -i license | wc -l) -lt 10 ]
then 
  # sleep 3
  # echo "[INFO] killing TS"
  # pkill ts3client_linux
  echo "[INFO] installing db"
  cp /app/settings.db /root/.ts3client/
fi
echo "[INFO] starting TS again"
./ts3client_linux_amd64 --no-sandbox "ts3server://ts.109tech.com?port=9987&nickname=BOOTYCALL&channel=Generico"


