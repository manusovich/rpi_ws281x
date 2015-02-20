#!/bin/bash

echo "Read forecast"
curl https://aladdin-service.herokuapp.com/forecast > /home/pi/rpi_ws281x/forecast
echo "Kill old instance..."
pkill test
echo "Run new instance..."
exec /home/pi/rpi_ws281x/test &
echo "Start pooling for changes"


C=0
N=0
while true; do
    C=$((C+1))
    N=$((C%180))

    if [ $N -eq 0 ]
    then
        echo "Update forecast... "
        curl https://aladdin-service.herokuapp.com/forecast > /home/pi/rpi_ws281x/forecast

        echo "Check repository... "
        cd /home/pi/rpi_ws281x
        git fetch > build_log.txt 2>&1

        if [ -s build_log.txt ]
        then
            echo "Application code has been changed. Getting changes..."
            cd /home/pi/rpi_ws281x
            git pull
            echo "Bulding application..."
            scons
            echo "Kill old application..."
            pkill test
            echo "Launch new application..."
            exec /home/pi/rpi_ws281x/test &
            echo "Done"
        else
            echo "No changes in the repository ($N)"
        fi
    fi

	sleep 10s
done
