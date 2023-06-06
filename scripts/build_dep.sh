#!/bin/bash

ALPINE_VERSION=3.18
COMPONENT=$1
ARCH=$2

podman run -it --rm -v ./build:/out -v ./scripts/build_$COMPONENT.sh:/build.sh:ro --arch $ARCH alpine:$ALPINE_VERSION /build.sh
