#!/bin/bash

echo "Kill old instance..."
sudo pkill test
echo "Run new instance..."
sudo /home/pi/rpi_ws281x/test &
echo "Start pooling for changes"

counter=0
((counter++))

increase=1

while true; do
	#if [["$counter" -gt 10]]
	#then
		sudo curl https://aladdin-service.herokuapp.com/forecast > /home/pi/rpi_ws281x/forecast
		counter=0
#	fi

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
   		echo "NO CHANGES ($counter)"
	fi

	counter = (($counter+1))
	sleep 10s
done
