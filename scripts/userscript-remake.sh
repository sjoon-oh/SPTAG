#!/bin/bash
#
# github.com/sjoon-oh/SPTAG
# Author: Sukjoon Oh, sjoon@kaist.ac.kr
# 

project_home="SPTAG"
project_home="SPTAG-cache"
workspace_home=`basename $(pwd)`

warning='\033[0;31m[WARNING]\033[0m '
normalc='\033[0;32m[MESSAGE]\033[0m '

# args=$@

#
# Setting proj home
if [[ ${workspace_home} != ${project_home} ]]; then
    printf "${warning}Currently in wrong directory: `pwd`\n"
    exit
fi

mkdir -p build
mkdir -p trace

rm build/*

cd build && cmake .. && make -j


