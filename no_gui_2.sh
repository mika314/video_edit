#!/bin/bash

FPS=30
#SampleRate=48000
SampleRate=44100

rootdir=`dirname $0`
echo Detect silence in the video
$rootdir/silence_detector/silence_detector $1
echo Remove frames from the video
ffmpeg -i $1 -f yuv4mpegpipe -r $FPS - | $rootdir/frame_remover/frame_remover $1_rm.txt $SampleRate | ffmpeg -f yuv4mpegpipe -i - -b 3000k -threads 16 `basename $1 .mp4`_noaudio.mp4
mv $1.s16l $1.raw
echo Mix video and audio
ffmpeg -i `basename $1 .mp4`_noaudio.mp4 -f s16le -ac 1 -ar $SampleRate -i $1.raw -map 0:0 -map 1:0 -vcodec copy -ab 128k -strict experimental -acodec aac `basename $1 .mp4`_mix.mp4
echo Move atom
qt-faststart `basename $1 .mp4`_mix.mp4 `basename $1 .mp4`.mov
echo Clean up


rm `basename $1 .mp4`_mix.mp4
rm $1.raw
rm $1_rm.txt
rm `basename $1 .mp4`_noaudio.mp4
