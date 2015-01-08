#!/bin/bash

git fetch > build_log.txt 2>&1

if [ -s build_log.txt ]
then
   echo "CHANGED"
else
   echo "NO CHANGES"
fi
