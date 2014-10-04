/*
 * Problem 3 --

Write a function that probes the PCI bus for all the devices 
connected.

a. Display the Device ID and Vendor ID of all the devices attached
on each PCI bus of Lab targets, and also print the bus and function
number for each device.

b.Identify on which bus is the Cirrus Logic CS4281 Sound card
 attached?
 */

#define DEVICE_ID 0x6005
#define VENDOR_ID 0x1013

void get_pci_devices()
{
	int i=0;
	pciConfigTopoShow();
	
	for (i=0;i<256;i++)
	{
	 pciDeviceShow(i); /*Prints out all 256 possibilities that pci can support*/
	}
}


/* As seen in the datasheet page 13 of the Audio Decoder Spec which
 * Specifies the Device and Vendor ID for the Cirrus Logic Card
 * are Device_ID 0x6005  Vendor_ID 0x1013
 */

void get_cirrus_info()
{
	pciFindDeviceShow(VENDOR_ID,DEVICE_ID,0);
}
