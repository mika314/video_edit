#!/bin/bash

rootdir=`dirname $0`

convert ~/dl/lmte.png -fill blue -font DejaVu-Sans-Mono-Book -pointsize 60 -draw "text 80,640 '$2'" `basename $1 .mp4`.png
$rootdir/silence_detector/silence_detector $1
avconv -i $1 -f yuv4mpegpipe - | $rootdir/frame_remover/frame_remover $1_rm.txt 48000 | $rootdir/prepend_frame/prepend_frame `basename $1 .mp4`.png 9188 | yuvview | avconv -f yuv4mpegpipe -i - -b 10000k -threads 8 `basename $1 .mp4`_noaudio.mp4
mv $1.s16l $1.raw
sox -r 48k -e signed -b 16 -c 1 $1.raw $1_compressed.raw compand 0.3,1 6:-55,-50,-20 -5 -90 0.2
$rootdir/add_bg/add_bg $1_compressed.raw ~/dl/Microchip.raw > $1_bg.raw
cat ~/dl/jingle.raw $1_bg.raw > $1_jg_bg.raw
avconv -i `basename $1 .mp4`_noaudio.mp4 -f s16le -ac 1 -ar 48000 -i $1_jg_bg.raw -map 0:0 -map 1:0 -vcodec copy -ab 64k `basename $1 .mp4`_mix.mp4
qt-faststart `basename $1 .mp4`_mix.mp4 `basename $1 .mp4`.mov
rm `basename $1 .mp4`_mix.mp4
rm $1.raw
rm $1_compressed.raw
rm $1_bg.raw
rm $1_jg_bg.raw
rm $1_rm.txt
rm `basename $1 .mp4`_noaudio.mp4
