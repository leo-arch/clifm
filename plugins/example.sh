#!/bin/sh

# This is a CliFM example action script

echo "Profile: $CLIFM_PROFILE
PWD: "$(pwd)"
Script: $(basename "$0")
Parameters: "$@""

exit 0
