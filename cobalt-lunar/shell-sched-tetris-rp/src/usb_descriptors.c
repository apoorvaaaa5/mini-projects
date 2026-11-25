#include <usb_descriptors.h>

/* USB 2.0 Standard Device Descriptor (18 bytes) */
const uint8_t usb_dev_desc[] = {
	18,                         /* bLength */
	USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
	0x00, 0x02,                 /* bcdUSB 2.00 */
	0xEF,                       /* bDeviceClass: Miscellaneous Device Class (0xEF) */
	0x02,                       /* bDeviceSubClass: Common Class (0x02) */
	0x01,                       /* bDeviceProtocol: Interface Association Descriptor (0x01) */
	64,                         /* bMaxPacketSize0 */
	0x8A, 0x2E,                 /* idVendor: 0x2E8A (Raspberry Pi) */
	0x0A, 0x00,                 /* idProduct: 0x000A (RP2040 CDC) */
	0x00, 0x01,                 /* bcdDevice 1.00 */
	1,                          /* iManufacturer */
	2,                          /* iProduct */
	3,                          /* iSerialNumber */
	1                           /* bNumConfigurations */
};
const uint32_t usb_dev_desc_len = sizeof(usb_dev_desc);

/* USB CDC ACM Configuration Descriptor (75 bytes with IAD) */
const uint8_t usb_config_desc[] = {
	/* Configuration Descriptor (9 bytes) */
	9,                          /* bLength */
	USB_DESC_TYPE_CONFIG,       /* bDescriptorType */
	75, 0,                      /* wTotalLength: 75 bytes */
	2,                          /* bNumInterfaces: 2 (CDC Control + CDC Data) */
	1,                          /* bConfigurationValue */
	0,                          /* iConfiguration (none) */
	0x80,                       /* bmAttributes: Bus powered */
	250,                        /* bMaxPower: 500mA (250 * 2mA) */

	/* Interface Association Descriptor (8 bytes) */
	8,                          /* bLength */
	USB_DESC_TYPE_IAD,          /* bDescriptorType: 0x0B */
	0,                          /* bFirstInterface: Interface 0 */
	2,                          /* bInterfaceCount: 2 interfaces */
	0x02,                       /* bFunctionClass: Communication (CDC) */
	0x02,                       /* bFunctionSubClass: Abstract Control Model (ACM) */
	0x00,                       /* bFunctionProtocol: None */
	0,                          /* iFunction */

	/* Interface 0: CDC Communication Control (9 bytes) */
	9,                          /* bLength */
	USB_DESC_TYPE_INTERFACE,    /* bDescriptorType */
	0,                          /* bInterfaceNumber */
	0,                          /* bAlternateSetting */
	1,                          /* bNumEndpoints: 1 (Notification EP) */
	0x02,                       /* bInterfaceClass: CDC */
	0x02,                       /* bInterfaceSubClass: Abstract Control Model (ACM) */
	0x00,                       /* bInterfaceProtocol: None */
	0,                          /* iInterface */

	/* CDC Header Functional Descriptor (5 bytes) */
	5,                          /* bLength */
	0x24,                       /* bDescriptorType: CS_INTERFACE */
	0x00,                       /* bDescriptorSubtype: Header */
	0x20, 0x01,                 /* bcdCDC: 1.20 */

	/* CDC Call Management Functional Descriptor (5 bytes) */
	5,                          /* bLength */
	0x24,                       /* bDescriptorType: CS_INTERFACE */
	0x01,                       /* bDescriptorSubtype: Call Management */
	0x00,                       /* bmCapabilities: Device does not handle call mgmt */
	1,                          /* bDataInterface: Interface 1 is data */

	/* CDC ACM Functional Descriptor (4 bytes) */
	4,                          /* bLength */
	0x24,                       /* bDescriptorType: CS_INTERFACE */
	0x02,                       /* bDescriptorSubtype: ACM */
	0x06,                       /* bmCapabilities: Support Set/Get Line Coding, Send Break */

	/* CDC Union Functional Descriptor (5 bytes) */
	5,                          /* bLength */
	0x24,                       /* bDescriptorType: CS_INTERFACE */
	0x06,                       /* bDescriptorSubtype: Union */
	0,                          /* bMasterInterface: Interface 0 */
	1,                          /* bSlaveInterface0: Interface 1 */

	/* Endpoint 1 IN: Notification Endpoint (7 bytes) */
	7,                          /* bLength */
	USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType */
	0x81,                       /* bEndpointAddress: EP 1 IN */
	0x03,                       /* bmAttributes: Interrupt */
	16, 0,                      /* wMaxPacketSize: 16 bytes */
	16,                         /* bInterval: 16 ms polling */

	/* Interface 1: CDC Data (9 bytes) */
	9,                          /* bLength */
	USB_DESC_TYPE_INTERFACE,    /* bDescriptorType */
	1,                          /* bInterfaceNumber */
	0,                          /* bAlternateSetting */
	2,                          /* bNumEndpoints: 2 (Bulk IN + Bulk OUT) */
	0x0A,                       /* bInterfaceClass: CDC Data */
	0x00,                       /* bInterfaceSubClass */
	0x00,                       /* bInterfaceProtocol */
	0,                          /* iInterface */

	/* Endpoint 2 OUT: Bulk Data from Host (7 bytes) */
	7,                          /* bLength */
	USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType */
	0x02,                       /* bEndpointAddress: EP 2 OUT */
	0x02,                       /* bmAttributes: Bulk */
	64, 0,                      /* wMaxPacketSize: 64 bytes */
	0,                          /* bInterval: Ignored for bulk */

	/* Endpoint 2 IN: Bulk Data to Host (7 bytes) */
	7,                          /* bLength */
	USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType */
	0x82,                       /* bEndpointAddress: EP 2 IN */
	0x02,                       /* bmAttributes: Bulk */
	64, 0,                      /* wMaxPacketSize: 64 bytes */
	0                           /* bInterval: Ignored for bulk */
};
const uint32_t usb_config_desc_len = sizeof(usb_config_desc);

/* String Descriptor 0: Supported Languages (English 0x0409) */
const uint8_t usb_string_lang[] = {
	4,
	USB_DESC_TYPE_STRING,
	0x09, 0x04
};
const uint32_t usb_string_lang_len = sizeof(usb_string_lang);

/* String Descriptor 1: Manufacturer "Praxis" */
const uint8_t usb_string_mfr[] = {
	14,
	USB_DESC_TYPE_STRING,
	'P', 0, 'r', 0, 'a', 0, 'x', 0, 'i', 0, 's', 0
};
const uint32_t usb_string_mfr_len = sizeof(usb_string_mfr);

/* String Descriptor 2: Product "RPI Pico" */
const uint8_t usb_string_prod[] = {
	18,
	USB_DESC_TYPE_STRING,
	'R', 0, 'P', 0, 'I', 0, ' ', 0, 'P', 0, 'i', 0, 'c', 0, 'o', 0
};
const uint32_t usb_string_prod_len = sizeof(usb_string_prod);

/* String Descriptor 3: Serial Number "123456" */
const uint8_t usb_string_serial[] = {
	14,
	USB_DESC_TYPE_STRING,
	'1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0
};
const uint32_t usb_string_serial_len = sizeof(usb_string_serial);
