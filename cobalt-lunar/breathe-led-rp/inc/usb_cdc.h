#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>

void usb_init(void);
void usb_poll(void);
int  usb_cdc_is_connected(void);
int  usb_cdc_write(const uint8_t *data, uint32_t len);
int  usb_cdc_read(uint8_t *data, uint32_t max_len);

#endif /* USB_CDC_H */
