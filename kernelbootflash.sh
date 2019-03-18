#!/bin/bash

# 메모리 카드 위치 인자 못받으면 종료.
sudo echo "script start"

if [[ -n "$1" ]]
then
    echo "build start at $1"
else
    echo "usage : $0 <memorycard device>"
    echo -e "if you don't want flashing to sdcard, press y \c"
    read word
    if [[ x"$word" == x"y" ]]
    then
        echo "build kernel img"
        ./build-rpi3-arm64.sh
        echo "build boot img"
        sudo ./scripts/mkbootimg_rpi3.sh
        echo "make tar file"
        tar -zcvf tizen-unified_20181024.1_iot-boot-arm64-rpi3.tar.gz boot.img modules.img
        echo "script ended."
        exit 0
    fi
    echo "script terminate."
    exit 1
fi

if [[ "$1" =~ "sda" ]]
then
    echo "warning! you try to flashing at hard disk!"
    echo "script terminate."
    exit 1
fi


echo "build kernel img"
./build-rpi3-arm64.sh
echo "build boot img"
sudo ./scripts/mkbootimg_rpi3.sh
echo "make tar file"
tar -zcvf tizen-unified_20181024.1_iot-boot-arm64-rpi3.tar.gz boot.img modules.img
echo "flashing to sd card"
sudo ./flash-sdcard.sh "$1"
echo "finish job"
exit 0
