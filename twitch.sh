#!/bin/bash
set -e
m1=$(pactl load-module module-null-sink sink_name=mix)
# m2=$(pactl load-module module-loopback sink=mix source=alsa_output.usb-Blue_Microphones_Yeti_Stereo_Microphone-00.analog-stereo.monitor)
# m3=$(pactl load-module module-loopback sink=mix source=alsa_input.usb-Blue_Microphones_Yeti_Stereo_Microphone-00.analog-stereo)
m2=$(pactl load-module module-loopback sink=mix source=alsa_output.usb-Kingston_HyperX_Virtual_Surround_Sound_00000000-00.analog-stereo.monitor)
m3=$(pactl load-module module-loopback sink=mix source=alsa_input.usb-Kingston_HyperX_Virtual_Surround_Sound_00000000-00.analog-stereo)
function cleanup {
  echo "unload pa modules"
  pactl unload-module $m3
  pactl unload-module $m2
  pactl unload-module $m1
}
trap cleanup EXIT
screen_capture 1080 | ffmpeg -i - -f pulse -ac 1 -ar 44100 -i mix.monitor -map 0:0 -map 1:0 -b:v 2000k -vcodec libx264 -x264-params keyint=60:scenecut=0:nal-hrd=cbr -preset:v ultrafast -threads 8 -strict experimental -ab 256k -ac 1 -f flv "rtmp://live-sjc.twitch.tv/app/$TWITCH_STREAM_KEY"
