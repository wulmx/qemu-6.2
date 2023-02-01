#!/bin/sh

Target_list=""
enable_cpu_type=""

if [ $# -ge 1 ] && [ "$1" == "ARM" ]; then
    Target_list="--target-list=aarch64-softmmu"
    cputype="ARM"
    targetpath="aarch64-softmmu"
else
    Target_list="--target-list=x86_64-softmmu"
    targetpath="x86_64-softmmu"                                                                                  
fi

echo "target_list:"$Target_list
./configure --enable-trace-backends=log --enable-kvm --enable-debug --enable-debug-info --enable-vnc ${Target_list}

