#!/bin/bash

cd build && make -j

cd ..
./Release/ssdserving custom-configs/sift1m-search-only.ini