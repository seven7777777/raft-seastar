#!/bin/env bash
set -e

export CC=clang
export CXX=clang++

build_type="release"
target="all"
while getopts "b:t:" opts; do
    case $opts in
        b) build_type=$OPTARG ;;
        t) target=$OPTARG ;;
        ?) ;;
    esac
done

build_type=$(echo $build_type | tr [A-Z] [a-z])
if [ "$build_type" == "release" ]; then
    build_type="Release"
elif [ "$build_type" == "debug" ]; then
    build_type="Debug"
else
    echo "invalid build type"
    exit 1
fi
NUM_CORES=$(cat /proc/cpuinfo | grep "processor" | wc -l)
echo "-- build with $NUM_CORES cores, type: $build_type, target: $target"

work_dir=`pwd`
build_dir="build/$(date '+%F/%H-%m-%S')/$build_type"
rm -rf $build_dir && mkdir -p $build_dir && cd $build_dir
cmake $work_dir -DCMAKE_BUILD_TYPE=$build_type
make -j $NUM_CORES $target
