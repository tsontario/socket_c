#!/bin/bash
# Run this from project root
make clean
docker build . -t csi4118:1.1
