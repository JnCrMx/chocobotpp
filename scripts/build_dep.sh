#!/bin/bash

UBUNTU_VERSION=noble
COMPONENT=$1
ARCH=$2

podman run -it --rm -v ./build:/out -v ./scripts/build_$COMPONENT.sh:/build.sh:ro --arch $ARCH ubuntu:$UBUNTU_VERSION /build.sh
