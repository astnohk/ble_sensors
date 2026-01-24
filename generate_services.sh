#!/bin/sh

rootdir=`pwd`

cat ./ble_sensors_server.service.template | sed -e "s#ROOTDIR#${rootdir}#g" > ./ble_sensors_server.service
