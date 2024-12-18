#!/usr/bin/env bash

adb -e shell -t '\
 echo 1 > /sys/kernel/tracing/events/binder/enable ; \
 echo 1 > /sys/kernel/tracing/tracing_on ; \
 cat /sys/kernel/tracing/trace_pipe \
'
