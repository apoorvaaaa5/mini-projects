#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>

#define USB_DIR_OUT                 0x00
#define USB_DIR_IN                  0x80

#define USB_REQ_TYPE_STANDARD       0x00
#define USB_REQ_TYPE_CLASS          0x20
#define USB_REQ_TYPE_VENDOR         0x40

#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_SET_INTERFACE       0x0B

/* CDC Specific Requests */
#define CDC_REQ_SET_LINE_CODING         0x20
#define CDC_REQ_GET_LINE_CODING         0x21
#define CDC_REQ_SET_CONTROL_LINE_STATE  0x22

/* Descriptor Types */
#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIG        0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_INTERFACE     0x04
#define USB_DESC_TYPE_ENDPOINT      0x05
#define USB_DESC_TYPE_IAD           0x0B

/* Setup Packet */
typedef struct __attribute__((packed)) {
	uint8_t  bmRequestType;
	uint8_t  bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} usb_setup_packet_t;

/* CDC Line Coding structure */
typedef struct __attribute__((packed)) {
	uint32_t dwDTERate;
	uint8_t  bCharFormat;
	uint8_t  bParityType;
	uint8_t  bDataBits;
} cdc_line_coding_t;

/* Descriptor pointers & lengths */
extern const uint8_t usb_dev_desc[];
extern const uint32_t usb_dev_desc_len;

extern const uint8_t usb_config_desc[];
extern const uint32_t usb_config_desc_len;

extern const uint8_t usb_string_lang[];
extern const uint32_t usb_string_lang_len;

extern const uint8_t usb_string_mfr[];
extern const uint32_t usb_string_mfr_len;

extern const uint8_t usb_string_prod[];
extern const uint32_t usb_string_prod_len;

extern const uint8_t usb_string_serial[];
extern const uint32_t usb_string_serial_len;

#endif /* USB_DESCRIPTORS_H */
