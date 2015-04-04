#!/bin/bash

rootdir=`dirname $0`
echo Detect silence in the video
$rootdir/silence_detector/silence_detector $1
echo Remove frames from the video
avconv -i $1 -f yuv4mpegpipe - | $rootdir/frame_remover/frame_remover $1_rm.txt 48000 | yuvview | avconv -f yuv4mpegpipe -i - -b 10000k -threads 8 `basename $1 .mp4`_noaudio.mp4
mv $1.s16l $1.raw
echo Normalize audio
sox -r 48k -e signed -b 16 -c 1 $1.raw $1_compressed.raw compand 0.3,1 6:-55,-50,-20 -5 -90 0.2
echo Mix video and audio
avconv -i `basename $1 .mp4`_noaudio.mp4 -f s16le -ac 1 -ar 48000 -i $1_compressed.raw -map 0:0 -map 1:0 -vcodec copy -ab 64k -strict experimental `basename $1 .mp4`_mix.mp4
echo Move atom
qt-faststart `basename $1 .mp4`_mix.mp4 `basename $1 .mp4`.mov
echo Clean up
rm `basename $1 .mp4`_mix.mp4
rm $1.raw
rm $1_compressed.raw
rm $1_rm.txt
rm `basename $1 .mp4`_noaudio.mp4
