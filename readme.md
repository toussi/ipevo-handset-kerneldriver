
**Requirements: 

Linux Kernel 2.6.x (Tested on 2.6.22) 

**Installation instructions : 

1.) compile and install 
make 
make install 


2.) prevent overlap with the usbhid driver. 

Add the following line to your /etc/modprobe.d/options 

options usbhid quirks=0x1778:0x0403:0x4


That should do the trick, plugin the device, and your keypad should work as a normal keyboard.
