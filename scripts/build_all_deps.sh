#!/bin/bash

for component in dpp libpqxx; do
    for arch in amd64 arm64; do
        ./scripts/build_dep.sh $component $arch
    done
done
