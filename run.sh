#!/bin/bash

make project
if [[ $? -eq 0 ]]; then
    ./run
fi
