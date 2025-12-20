# Start from Ubuntu base image
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV DISPLAY=:99
#ENV QT_QUICK_BACKEND=software
#suggested by chatgpt?
ENV TS3CLIENT_NO_CRASHDUMP=1 

# Install necessary packages
RUN apt-get update && apt-get install -y \
    xvfb \
    xinit \
    x11-utils \
    x11-apps \
    x11-xkb-utils \
    wmctrl \
    fluxbox \
    libsoxr-dev \
    libiniparser-dev \
    wget \
    make \
    gcc \
    build-essential

# Install for debugging
RUN apt-get update && apt-get install -y \
    x11vnc

#other dependencies suggested by chatgpt for ts
RUN apt-get update && apt-get install -y \
    libasound2 \
    libatk1.0-0 \
    libcups2 \
    libdbus-1-3 \
    libdbus-glib-1-2 \
    libevent-dev \
    libfontconfig1\
    libfreetype6 \
    libglib2.0-0 \
    libgtk2.0-0 \
    libice6 \
    liblcms2-2 \
    libnss3 \
    libpci3 \
    libpng16-16 \
    libpulse0 \
    libsm6 \
    libx11-6 \
    libx11-xcb1 \
    libxcb1 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-shm0 \
    libxcb-util1 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb-xkb1 \
    libxcomposite1 \
    libxcursor1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxi6 \
    libxkbcommon0 \
    libxkbcommon-x11-0\
    libxrandr2 \
    libxrender1 \
    libxslt1.1 \
    libxtst6 

RUN apt update && apt install -y \
    libgl1-mesa-glx \
    libglu1-mesa \
    libgl1-mesa-dri \
    mesa-utils 

RUN apt-get update && apt-get install -y xserver-xorg xserver-xorg-video-dummy

RUN apt update && apt install -y pulseaudio
RUN apt-get update && apt-get install -y libc++abi1
RUN apt-get update && apt-get install -y sqlite3
# RUN apt-get update && apt-get install -y \
#     libgl1-mesa-glx libgl1-mesa-dri \
#     libdbus-1-3 libfontconfig1 libfreetype6 libxcomposite1 libxcursor1 libxdamage1 libxinerama1 libxss1 

#ENV QT_QPA_PLATFORM=xcb

WORKDIR /
RUN wget https://files.teamspeak-services.com/releases/client/3.6.2/TeamSpeak3-Client-linux_amd64-3.6.2.run -O "/ts3.run"
RUN chmod +x /ts3.run && yes | PAGER=cat /ts3.run --nox11
RUN mkdir /TeamSpeak3-Client-linux_amd64/plugins/

COPY ts3client-pluginsdk/ /app
COPY docker/xorg.conf /etc/X11/xorg.conf

WORKDIR /app
RUN make
RUN cp /app/src/utils/at_help.txt /TeamSpeak3-Client-linux_amd64/

# audio and dbus setup
RUN dbus-uuidgen > /etc/machine-id



#CMD ["Xvfb", ":99", "-screen", "0", "640x480x8", "-nolisten", "tcp"]
#CMD ["Xorg", ":99", "-noreset", "+extension", "GLX", "+extension", "RANDR", "+extension", "RENDER", "-logfile", "/tmp/xorg.log", "-config", "/etc/X11/xorg.conf"]
CMD ["/app/startscript.sh"]
