#!/bin/bash

avconv -i $1 -f yuv4mpegpipe -pix_fmt yuv420p - | `dirname $0`/frame_remover $1_rm.txt 48000 | avconv -f yuv4mpegpipe -i - -b 10000k -threads 8 `basename $1 .mp4`_noaudio.mp4
avconv -i `basename $1 .mp4`_noaudio.mp4 -f s16le -ac 1 -ar 48000 -i $1.s16l -map 0:0 -map 1:0 -vcodec copy -ab 64k -strict experimental `basename $1 .mp4`_mix.mp4
