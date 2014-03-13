#!/bin/bash

rootdir=`dirname $0`

$rootdir/silence_detector/silence_detector $1
avconv -i $1 -f yuv4mpegpipe - | $rootdir/frame_remover/frame_remover $1_rm.txt 48000 | yuvview | avconv -f yuv4mpegpipe -i - -b 10000k -threads 8 `basename $1 .mp4`_noaudio.mp4
$rootdir/add_bg/add_bg $1.s16l ~/dl/lmte_bg.raw > $1_bg.s16l
avconv -i `basename $1 .mp4`_noaudio.mp4 -f s16le -ac 1 -ar 48000 -i $1_bg.s16l -map 0:0 -map 1:0 -vcodec copy -ab 64k `basename $1 .mp4`_mix.mp4
qt-faststart `basename $1 .mp4`_mix.mp4 `basename $1 .mp4`.mov