#!/bin/bash

export XDG_RUNTIME_DIR=/tmp/runtime-root
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

# Start PulseAudio
pulseaudio --system --exit-idle-time=-1 &
export PULSE_SERVER=unix:/tmp/pulse.sock

# Wait for PulseAudio socket
for i in {1..20}; do
  if [ -S /tmp/pulse.sock ]; then break; fi
  echo "Waiting for PulseAudio..."
  sleep 0.5
done

# Start Xorg in the background
Xorg :99 -noreset +extension GLX +extension RANDR +extension RENDER -logfile /tmp/xorg.log -config /etc/X11/xorg.conf #&

# Wait for X to be ready
for i in {1..20}; do
  if xdpyinfo -display :99 > /dev/null 2>&1; then break; fi
  echo "Waiting for Xorg..."
  sleep 0.5
done

# Launch TS3
cd /TeamSpeak3-Client-linux_amd64/
DISPLAY=:99 ./ts3client_linux_amd64 --no-sandbox --license_accepted=1 "ts3server://ts.109tech.com?port=9987&nickname=BOOTYCALL&channel=Generico"

