#!/bin/bash

BRANCH=$1

cd /root
git clone https://github.com/BioDynaMo/biodynamo
export TRAVIS_OS_NAME="linux"
export TRAVIS=1
cd biodynamo/
git checkout $BRANCH
./travis.sh
