#!/bin/bash

cd build && make -j

post="60.5gb"

cd ..
# ./Release/ssdserving custom-configs/sift1m-search-only.ini
# ./Release/ssdserving custom-configs/spacev1b-search-only-log.ini

./Release/ssdserving custom-configs/spacev1b-search-only-log.ini > trace/spacev1b-out-${post}.txt
# ./Release/ssdserving-original custom-configs/spacev1b-search-only-log.ini > trace/spacev1b-out-${post}.txt

# ./Release/ssdserving custom-configs/sift1m-search-only.ini > trace/sift1m-out-${post}.txt
# ./Release/ssdserving-original custom-configs/sift1m-search-only.ini > trace/sift1m-out-${post}.txt

cd trace

mv cache-get-latency.csv cache-get-latency-${post}.csv 
mv cache-set-latency.csv cache-set-latency-${post}.csv 
mv cache-trace.csv cache-trace-${post}.csv 
mv offset-access-trace.csv offset-access-trace-${post}.csv 


