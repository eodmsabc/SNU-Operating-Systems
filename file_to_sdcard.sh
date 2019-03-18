#!/bin/bash
echo "this script mount, copy file and unmount automatecally."

if [[ -n "$1" ]]
then
    echo "copy $1"
else
    echo "usage : $0 <fileLocation> <mountlocation>"
    echo "script terminate"
    exit 1
fi

if [[ -n "$2" ]]
then
    echo "to $2 location"
else
    echo "usage : $0 <fileLocation> <mountlocation>"
    echo "script terminate"
    exit 1
fi

copypos="$2"
copypos+='/root'

echo "mounting.."
mount /dev/sdb2 /mnt/usb
echo "copy file to sdcard"
cp "$1" "$copypos"
echo "unmounting.."
umount /mnt/usb

echo "script end"
exit 0
