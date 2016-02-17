
/* ipevo : Driver for Ipevo USB handsets. Currently supports model vp710 
 * (The model without the LCD)
 *  
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 *  
 *  
*/
//#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/usb.h>
#include <linux/usb/input.h>

#define IPEVO_USB_VENDOR_ID 0x1778
#define IPEVO_USB_PRODUCT_ID 0x0403


#define DRIVER_VERSION "ipv-20080731"
#define DRIVER_AUTHOR "Daniel Toussaint"
#define DRIVER_DESC "Ipevo usb handset"

#define TOTALKEYS 30

struct keytable_s  {
	unsigned char pevo_key;
	unsigned int linput_key;
} keytable[TOTALKEYS] ={ { 0xb0, KEY_0 } ,
			{ 0xb1, KEY_1 } ,
		 	{ 0xb2, KEY_2 } ,
		 	{ 0xb3, KEY_3} , 
		 	{ 0xb4, KEY_4} ,
		 	{ 0xb5, KEY_5} , 
			{ 0xb6, KEY_6} ,
			{ 0xb7, KEY_7} , 
			{ 0xb8, KEY_8} ,
			{ 0xb9, KEY_9} ,
			{ 0xba,  KEY_KPASTERISK },
			{ 0xbb, KEY_LEFTSHIFT | KEY_3  }, //pound key # 
			{ 0x32, KEY_VOLUMEUP} , 
			{ 0x33, KEY_VOLUMEDOWN}, 
			{ 0x2f, KEY_MUTE},
			{ 0xf1, KEY_F1},  // three buttons at upper row
			{ 0xf2, KEY_F2},   // 
			{ 0xf3, KEY_F3}, // 
			{ 0x20, KEY_ENTER }, //call button 
			{ 0x1f , KEY_ESC}, //hangup button 
			{ 0x1e , KEY_DOWN } , 
			{ 0x1d , KEY_UP}, 
			{ 0x1c, KEY_PHONE }, //skype button   
			{ 0xc1, KEY_KPPLUS }, 
			{ 0xc3, KEY_HOMEPAGE } , // list button  
};

struct ipevo_dev {

	char 			name[128];
	char			phys[64];
	struct input_dev 	*idev;
	dma_addr_t 		data_dma;
	struct usb_device 	*udev;
	struct usb_interface 	*interface;
	struct usb_endpoint_descriptor *endpoint;
	struct urb		*irq_urb;
	unsigned char 		*input_buffer; 

};


static unsigned int ipevo_key_to_linput ( unsigned char inputkey) {

	int i;

	for ( i = 0; i< TOTALKEYS; i++) {
		if ( inputkey == keytable[i].pevo_key) return keytable[i].linput_key; 
	}
	return KEY_0;

}


static void ipevo_irq_callback (struct urb *urb , struct pt_regs *regs ) { 
	
	struct ipevo_dev *pevo = urb->context;
	int retret;

	switch ( urb->status) {
		case 0:
			break;
		case -ETIMEDOUT:
			printk("URB timed out ... \n");
			return;
		case -ECONNRESET:
		case -ENOENT:
		case -ESHUTDOWN:
			return;
		default: 
			return;
			
	}
	
	//input_regs (pevo->idev , regs);

	input_report_key( pevo->idev, ipevo_key_to_linput(pevo->input_buffer[1]) , 1);
	input_report_key( pevo->idev, ipevo_key_to_linput(pevo->input_buffer[1]) , 0);

	input_sync(pevo->idev);
		
	retret = usb_submit_urb( pevo->irq_urb, GFP_ATOMIC);

}



static int pevo_open (struct input_dev *input) {


	struct ipevo_dev *pevo = input->private; 
	
	pevo->irq_urb->dev = pevo->udev;	

	if (usb_submit_urb(pevo->irq_urb, GFP_ATOMIC)) 
		return -EIO;
	

	return 0; 
}


static void pevo_close (struct input_dev *input){
	
	struct ipevo_dev *pevo = input->private;

	usb_kill_urb(pevo->irq_urb); 

}

static int pevo_alloc_buffers (struct usb_device *udev, struct ipevo_dev *pevo 	) {

	pevo->input_buffer = usb_buffer_alloc(udev, 7 , GFP_KERNEL,  &pevo->data_dma);
	if ( !pevo->input_buffer ) return -1; 

	return 0;


}


static void pevo_free_buffers( struct usb_device *udev, struct ipevo_dev *pevo) {

		if (pevo->input_buffer) 
			usb_buffer_free (udev,  7, pevo->input_buffer, pevo->data_dma);
	
}

static int  usb_probe ( struct usb_interface *intf, const struct usb_device_id *id ) {

	struct usb_device *udev = interface_to_usbdev (intf);
	struct input_dev *inputdevice;
	struct ipevo_dev *pevo;

	struct usb_host_interface *hostinterface;
	struct usb_endpoint_descriptor *endpoint;
	int pipe;
	int i;

	hostinterface = intf->cur_altsetting;

	endpoint = &hostinterface->endpoint[0].desc;
	if (!(endpoint->bEndpointAddress & USB_DIR_IN))
	                 return -EIO;

	pevo = kzalloc (sizeof(struct ipevo_dev), GFP_KERNEL);
	if (!pevo) return -ENOMEM;


	pevo->udev= udev;
	inputdevice= input_allocate_device();
	if (!inputdevice) goto bail2;
	pevo->idev = inputdevice;


	if (pevo_alloc_buffers(udev, pevo) ) goto bail1; 
	
	
	//sysfs
	if (udev->manufacturer) strlcpy(pevo->name , udev->manufacturer, sizeof(pevo->name));
	if (udev->product) {
		if (udev->manufacturer) strlcat(pevo->name , " " , sizeof(pevo->name));
		 strlcat (pevo->name, udev->product , sizeof(pevo->name));
	}


	
	usb_make_path(udev, pevo->phys , sizeof(pevo->phys));
	strlcpy(pevo->phys, "/input0" , sizeof(pevo->phys));

	inputdevice->name = pevo->name;
	inputdevice->phys = pevo->phys;
	usb_to_input_id(udev, &inputdevice->id);
	inputdevice->cdev.dev = &intf->dev;
	inputdevice->private = pevo;
	
	
	inputdevice->open = pevo_open; 
	inputdevice->close = pevo_close; 
	
	inputdevice->evbit[0] = BIT(EV_KEY);
	for (i=0; i < TOTALKEYS; i++) {
		//set_bit(KEY_1, inputdevice->keybit);
		set_bit( keytable[i].linput_key, inputdevice->keybit);
	}


	pipe = usb_rcvintpipe( udev, endpoint->bEndpointAddress);
	pevo->irq_urb = usb_alloc_urb( 0 ,GFP_KERNEL);
	usb_fill_int_urb( pevo->irq_urb , pevo->udev , pipe , pevo->input_buffer, 7 , ipevo_irq_callback, pevo, endpoint->bInterval);

	// 	
	
	/*Finally register the input device */
	input_register_device(pevo->idev);


	usb_set_intfdata( intf, pevo);
	return 0;

bail1:
	pevo_free_buffers(udev, pevo);
bail2:	
	input_free_device(inputdevice);

	kfree(pevo);
	return -ENOMEM;

}


static void usb_disconnect ( struct usb_interface *intf ) {

	 struct ipevo_dev *pevo = usb_get_intfdata(intf);

	
	 if (pevo) {
	 	usb_kill_urb(pevo->irq_urb);
		input_unregister_device(pevo->idev);
		usb_free_urb(pevo->irq_urb);
		pevo_free_buffers( interface_to_usbdev(intf) , pevo);
		kfree(pevo);
	}


}


struct driver_info {
        char *name;
};

static const struct driver_info info_vp170 = {
        .name   = "Ipevo VP170",
};



static const struct usb_device_id usb_table [] = {
{

	.match_flags = USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_INT_INFO,
	.idVendor = IPEVO_USB_VENDOR_ID,
	.idProduct = IPEVO_USB_PRODUCT_ID,
	.bInterfaceClass        = USB_CLASS_HID,
	.bInterfaceSubClass     = 0,
	.bInterfaceProtocol     = 0,
	.driver_info            = (kernel_ulong_t)&info_vp170
	}, 
	{ } 
};


static struct usb_driver ipevo_driver = {
     	.name           = "ipevo",
	.probe          = usb_probe,
	.disconnect     = usb_disconnect,
	.id_table       = usb_table,
};


MODULE_DEVICE_TABLE( usb, usb_table );

static int __init ipevo_dev_init(void)
{
	
	int result = usb_register(&ipevo_driver);
	if (result == 0) info(DRIVER_DESC ":" DRIVER_VERSION);
	return result;
	
}

static void __exit ipevo_dev_exit(void)
{
	usb_deregister(&ipevo_driver);
}

module_init(ipevo_dev_init);
module_exit(ipevo_dev_exit);

MODULE_DEVICE_TABLE (usb, usb_table);


MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
