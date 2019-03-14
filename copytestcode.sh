echo "mount" 
mount /dev/mmcblk0p2 mountdir
echo "copy"
cp test/test mountdir/root/test
echo "unmount"
umount mountdir
