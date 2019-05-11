#!/bin/bash
echo "this script mount, copy file or folder and unmount automatecally."
echo "this script needs sudo, so you should execute this script with sudo."

if [[ -n "$1" ]]
then
    echo "copy $1"
else
    echo "usage : $0 <file or folder Location> <devicelocation> <mountlocation>"
    echo "script terminate"
    exit 1
fi

if [[ -n "$2" ]]
then
    echo "to $2 device"
else
    echo "usage : $0 <file or folder Location> <devicelocation> <mountlocation>"
    echo "script terminate"
    exit 1
fi

if [[ -n "$3" ]]
then
    echo "with $3 mount position"
else
    echo "usage : $0 <file or folder Location> <devicelocation> <mountlocation>"
    echo "script terminate"
    exit 1
fi


copypos="$3"
copypos+='/root'

echo "mounting.."
mount "$2" "$3" || { echo "mount failure!"; exit 1; }
echo "copy folder/file to sdcard"
cp -R "$1" "$copypos" || { echo "copy failed!"; exit 1; }
echo "unmounting.."
umount "$3" || { echo "umount failure!"; exit 1; }

echo "script end"
exit 0

