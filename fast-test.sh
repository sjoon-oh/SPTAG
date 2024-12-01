#!/bin/bash

rm log/*.out

# ./Release/ssdserving configs/sift1m.ini \
#     --cache-policy "LRU" \
#     --cache-size 1073741824 \
#     > log/fast-test-sift1m-lru.out

# LOGDIR=log/sift1m-lru
# mkdir -p ${LOGDIR}

# mv access-distribution.csv ${LOGDIR}/
# mv access-location.csv ${LOGDIR}/
# mv access-history.csv ${LOGDIR}/

# mv cache-get-stats.csv ${LOGDIR}/
# mv cache-delay-stats.csv ${LOGDIR}/

# mv cache-get-elapsed.csv ${LOGDIR}/
# mv cache-delay-elapsed.csv ${LOGDIR}/


# ./Release/ssdserving configs/sift1m.ini \
#     --cache-policy "LFU" \
#     --cache-size 1073741824 \
#     > log/fast-test-sift1m-lfu.out

# LOGDIR=log/sift1m-lru
# mkdir -p ${LOGDIR}

# mv access-distribution.csv ${LOGDIR}/
# mv access-location.csv ${LOGDIR}/
# mv access-history.csv ${LOGDIR}/

# mv cache-get-stats.csv ${LOGDIR}/
# mv cache-delay-stats.csv ${LOGDIR}/

# mv cache-get-elapsed.csv ${LOGDIR}/
# mv cache-delay-elapsed.csv ${LOGDIR}/


# ./Release/ssdserving configs/sift1m.ini \
#     --cache-policy "FIFO" \
#     --cache-size 1073741824 \
#     > log/fast-test-sift1m-fifo.out

# LOGDIR=log/sift1m-lru
# mkdir -p ${LOGDIR}

# mv access-distribution.csv ${LOGDIR}/
# mv access-location.csv ${LOGDIR}/
# mv access-history.csv ${LOGDIR}/

# mv cache-get-stats.csv ${LOGDIR}/
# mv cache-delay-stats.csv ${LOGDIR}/

# mv cache-get-elapsed.csv ${LOGDIR}/
# mv cache-delay-elapsed.csv ${LOGDIR}/


# SPACEV1B
# ./Release/ssdserving configs/spacev1b.ini \
#     --cache-policy "LRU" \
#     --cache-size 1073741824 \
#     > log/fast-test-spacev1b-lru.out

# ./Release/ssdserving configs/spacev1b.ini \
#     --cache-policy "LFU" \
#     --cache-size 1073741824 \
#     > log/fast-test-spacev1b-lfu.out

# ./Release/ssdserving configs/spacev1b.ini \
#     --cache-policy "FIFO" \
#     --cache-size 1073741824 \
#     > log/fast-test-spacev1b-fifo.out


# SPACEV1B
# ./ssdserving configs/spacev1b-extended.ini > log/original-spacev1b-extended.out

./Release/ssdserving configs/spacev1b-extended.ini \
    --cache-policy "LRU" \
    --cache-size 1073741824 \
    > log/fast-test-spacev1b-extended-lru.out

LOGDIR=log/spacev1b-extended-lru
mkdir -p ${LOGDIR}

mv access-distribution.csv ${LOGDIR}/
mv access-location.csv ${LOGDIR}/
mv access-history.csv ${LOGDIR}/

mv cache-get-stats.csv ${LOGDIR}/
mv cache-delay-stats.csv ${LOGDIR}/

mv cache-get-elapsed.csv ${LOGDIR}/
mv cache-delay-elapsed.csv ${LOGDIR}/

./Release/ssdserving configs/spacev1b-extended.ini \
    --cache-policy "LFU" \
    --cache-size 1073741824 \
    > log/fast-test-spacev1b-extended-lfu.out

LOGDIR=log/spacev1b-extended-lfu
mkdir -p ${LOGDIR}

mv access-distribution.csv ${LOGDIR}/
mv access-location.csv ${LOGDIR}/
mv access-history.csv ${LOGDIR}/

mv cache-get-stats.csv ${LOGDIR}/
mv cache-delay-stats.csv ${LOGDIR}/

mv cache-get-elapsed.csv ${LOGDIR}/
mv cache-delay-elapsed.csv ${LOGDIR}/

./Release/ssdserving configs/spacev1b-extended.ini \
    --cache-policy "FIFO" \
    --cache-size 1073741824 \
    > log/fast-test-spacev1b-extended-fifo.out

LOGDIR=log/spacev1b-extended-fifo
mkdir -p ${LOGDIR}

mv access-distribution.csv ${LOGDIR}/
mv access-location.csv ${LOGDIR}/
mv access-history.csv ${LOGDIR}/

mv cache-get-stats.csv ${LOGDIR}/
mv cache-delay-stats.csv ${LOGDIR}/

mv cache-get-elapsed.csv ${LOGDIR}/
mv cache-delay-elapsed.csv ${LOGDIR}/