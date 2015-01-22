#!/bin/bash

while true; do
	echo "Read forecast"
	sudo curl https://aladdin-service.herokuapp.com/forecast > /home/pi/rpi_ws281x/forecast
	sleep 30m
done
