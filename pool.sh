#!/bin/bash

echo "Read forecast"
curl https://aladdin-service.herokuapp.com/forecast > /home/pi/rpi_ws281x/forecast
echo "Kill old instance..."
pkill test
echo "Run new instance..."
exec /home/pi/rpi_ws281x/test &
echo "Start pooling for changes"

while true; do
	cd /home/pi/rpi_ws281x
	git fetch > build_log.txt 2>&1 

	if [ -s build_log.txt ]
	then
   		echo "CHANGED"
		cd /home/pi/rpi_ws281x
		git pull
		scons
		echo "Kill old app..."
		pkill test
		echo "Launch new app..."
		exec /home/pi/rpi_ws281x/test &
		echo "Done"
	else
   		echo "NO CHANGES"
	fi

	sleep 10s
done
