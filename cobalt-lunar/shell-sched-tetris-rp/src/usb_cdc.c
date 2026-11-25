#include <mmio.h>
#include <string.h>
#include <usb_descriptors.h>
#include <usb_cdc.h>

/* ==========================================================================
 * RP2040 USB Hardware Base Addresses & Registers
 * ========================================================================== */
#define USB_DPRAM_BASE              0x50100000
#define USB_REGS_BASE               0x50110000

/* Register Offsets */
#define USB_ADDR_ENDP               (USB_REGS_BASE + 0x00)
#define USB_MAIN_CTRL               (USB_REGS_BASE + 0x40)
#define USB_SIE_CTRL                (USB_REGS_BASE + 0x4C)
#define USB_SIE_STATUS              (USB_REGS_BASE + 0x50)
#define USB_BUFF_STATUS             (USB_REGS_BASE + 0x58)
#define USB_BUFF_CPU_SHOULD_HANDLE  (USB_REGS_BASE + 0x5C)
#define USB_MUXING                  (USB_REGS_BASE + 0x74)
#define USB_PWR                     (USB_REGS_BASE + 0x78)

/* DPRAM Offsets */
#define DPRAM_SETUP_PACKET          (USB_DPRAM_BASE + 0x00)
#define DPRAM_EP1_IN_CTRL           (USB_DPRAM_BASE + 0x08)
#define DPRAM_EP1_OUT_CTRL          (USB_DPRAM_BASE + 0x0C)
#define DPRAM_EP2_IN_CTRL           (USB_DPRAM_BASE + 0x10)
#define DPRAM_EP2_OUT_CTRL          (USB_DPRAM_BASE + 0x14)

#define DPRAM_EP0_IN_BUF_CTRL       (USB_DPRAM_BASE + 0x80)
#define DPRAM_EP0_OUT_BUF_CTRL      (USB_DPRAM_BASE + 0x84)
#define DPRAM_EP1_IN_BUF_CTRL       (USB_DPRAM_BASE + 0x88)
#define DPRAM_EP1_OUT_BUF_CTRL      (USB_DPRAM_BASE + 0x8C)
#define DPRAM_EP2_IN_BUF_CTRL       (USB_DPRAM_BASE + 0x90)
#define DPRAM_EP2_OUT_BUF_CTRL      (USB_DPRAM_BASE + 0x94)

#define DPRAM_EP0_BUF               (USB_DPRAM_BASE + 0x100)
#define DPRAM_EP1_IN_BUF            (USB_DPRAM_BASE + 0x140)
#define DPRAM_EP2_OUT_BUF           (USB_DPRAM_BASE + 0x180)
#define DPRAM_EP2_IN_BUF            (USB_DPRAM_BASE + 0x1C0)

/* Buffer control bit masks */
#define USB_BUF_CTRL_FULL           (1 << 15)
#define USB_BUF_CTRL_LAST           (1 << 14)
#define USB_BUF_CTRL_DATA_PID       (1 << 13)
#define USB_BUF_CTRL_AVAIL          (1 << 10)
#define USB_BUF_CTRL_LEN_MASK       0x3FF

/* Endpoint control bit masks */
#define EP_CTRL_ENABLE              (1 << 31)
#define EP_CTRL_INT_PER_BUF         (1 << 29)
#define EP_CTRL_TYPE_INTERRUPT      (3 << 26)
#define EP_CTRL_TYPE_BULK           (2 << 26)

/* Resets block */
#define RESETS_RESET                0x4000C000
#define RESETS_RESET_DONE           0x4000C008
#define RESETS_USBCTRL_BIT          (1 << 24)

/* RP2040 Atomic Register Manipulation Aliases */
#define REG_ALIAS_SET               0x2000
#define REG_ALIAS_CLR               0x3000

/* State variables */
static uint8_t  usb_dev_addr = 0;
static uint8_t  usb_pending_addr = 0;
static int      usb_configured = 0;
static int      usb_connected = 0;

/* USB CDC Circular RX Buffer */
#define USB_RX_BUF_SIZE 128
static uint8_t usb_rx_buf[USB_RX_BUF_SIZE];
static volatile uint16_t usb_rx_head = 0;
static volatile uint16_t usb_rx_tail = 0;

static uint8_t  ep0_in_pid = 1;
static uint8_t  ep0_dir_in = 0;
static uint8_t  ep2_in_pid = 0;
static uint8_t  ep2_out_pid = 0;

/* Multi-packet EP0 transfer tracking */
static const uint8_t *ep0_in_data = 0;
static uint32_t       ep0_in_remain = 0;

/* CDC Line Coding state (115200 8-N-1) */
static cdc_line_coding_t cdc_line_coding = {
	.dwDTERate   = 115200,
	.bCharFormat = 0, /* 1 stop bit */
	.bParityType = 0, /* None */
	.bDataBits   = 8
};

static void ep0_out_prime(uint32_t len)
{
	uint32_t ctrl = USB_BUF_CTRL_DATA_PID |
	                USB_BUF_CTRL_AVAIL |
	                (len & USB_BUF_CTRL_LEN_MASK);
	mmio32(DPRAM_EP0_OUT_BUF_CTRL) = ctrl;
}

static void ep0_in_send_chunk(void)
{
	uint32_t chunk = ep0_in_remain;
	if (chunk > 64) chunk = 64;

	if (ep0_in_data && chunk > 0) {
		memcpy((void *)DPRAM_EP0_BUF, ep0_in_data, chunk);
		ep0_in_data += chunk;
	}
	ep0_in_remain -= chunk;

	/* On RP2040, USB_BUF_CTRL_LAST is only for double-buffered mode.
	 * Do not set it for single-buffered endpoint transfers. */
	uint32_t ctrl = USB_BUF_CTRL_FULL |
	                (ep0_in_pid ? USB_BUF_CTRL_DATA_PID : 0) |
	                USB_BUF_CTRL_AVAIL |
	                (chunk & USB_BUF_CTRL_LEN_MASK);

	mmio32(DPRAM_EP0_IN_BUF_CTRL) = ctrl;
	ep0_in_pid ^= 1;
}

static void ep0_in_xfer(const uint8_t *data, uint32_t len)
{
	ep0_in_data = data;
	ep0_in_remain = len;
	ep0_in_send_chunk();
}

static void handle_get_descriptor(const usb_setup_packet_t *pkt)
{
	uint8_t desc_type = (uint8_t)(pkt->wValue >> 8);
	uint8_t desc_index = (uint8_t)(pkt->wValue & 0xFF);
	const uint8_t *desc = 0;
	uint32_t desc_len = 0;

	switch (desc_type) {
	case USB_DESC_TYPE_DEVICE:
		desc = usb_dev_desc;
		desc_len = usb_dev_desc_len;
		break;
	case USB_DESC_TYPE_CONFIG:
		desc = usb_config_desc;
		desc_len = usb_config_desc_len;
		break;
	case USB_DESC_TYPE_STRING:
		if (desc_index == 0) {
			desc = usb_string_lang;
			desc_len = usb_string_lang_len;
		} else if (desc_index == 1) {
			desc = usb_string_mfr;
			desc_len = usb_string_mfr_len;
		} else if (desc_index == 2) {
			desc = usb_string_prod;
			desc_len = usb_string_prod_len;
		} else if (desc_index == 3) {
			desc = usb_string_serial;
			desc_len = usb_string_serial_len;
		}
		break;
	default:
		break;
	}

	if (desc) {
		uint32_t send_len = desc_len;
		if (send_len > pkt->wLength) {
			send_len = pkt->wLength;
		}
		ep0_in_xfer(desc, send_len);
	} else {
		/* Stall EP0 */
		mmio32(DPRAM_EP0_IN_BUF_CTRL) = 0;
	}
}

static void handle_setup_packet(void)
{
	volatile usb_setup_packet_t *pkt = (volatile usb_setup_packet_t *)DPRAM_SETUP_PACKET;
	ep0_in_pid = 1;
	ep0_dir_in = (pkt->bmRequestType & 0x80) ? 1 : 0;

	if ((pkt->bmRequestType & 0x60) == USB_REQ_TYPE_STANDARD) {
		switch (pkt->bRequest) {
		case USB_REQ_GET_DESCRIPTOR:
			handle_get_descriptor((const usb_setup_packet_t *)pkt);
			break;

		case USB_REQ_SET_ADDRESS:
			usb_pending_addr = (uint8_t)(pkt->wValue & 0x7F);
			/* Send zero-length status handshake packet on EP0 IN */
			ep0_in_xfer(0, 0);
			break;

		case USB_REQ_SET_CONFIGURATION:
			usb_configured = 1;

			/* Configure EP1 IN (Interrupt notification endpoint) */
			mmio32(DPRAM_EP1_IN_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUF |
			                            EP_CTRL_TYPE_INTERRUPT | (DPRAM_EP1_IN_BUF - USB_DPRAM_BASE);

			/* Configure EP2 OUT (Bulk data from host) */
			mmio32(DPRAM_EP2_OUT_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUF |
			                             EP_CTRL_TYPE_BULK | (DPRAM_EP2_OUT_BUF - USB_DPRAM_BASE);

			/* Configure EP2 IN (Bulk data to host) */
			mmio32(DPRAM_EP2_IN_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUF |
			                            EP_CTRL_TYPE_BULK | (DPRAM_EP2_IN_BUF - USB_DPRAM_BASE);

			/* Prime EP2 OUT to receive data from host */
			ep2_out_pid = 0;
			ep2_in_pid = 0;
			mmio32(DPRAM_EP2_OUT_BUF_CTRL) = USB_BUF_CTRL_AVAIL | 64;

			/* Acknowledge configuration */
			ep0_in_xfer(0, 0);
			break;

		case USB_REQ_GET_CONFIGURATION:
			{
				uint8_t cfg = (uint8_t)usb_configured;
				ep0_in_xfer(&cfg, 1);
			}
			break;

		case USB_REQ_GET_STATUS:
			{
				uint16_t status = 0;
				ep0_in_xfer((const uint8_t *)&status, 2);
			}
			break;

		default:
			ep0_in_xfer(0, 0);
			break;
		}
	} else if ((pkt->bmRequestType & 0x60) == USB_REQ_TYPE_CLASS) {
		/* CDC Class Requests */
		switch (pkt->bRequest) {
		case CDC_REQ_GET_LINE_CODING:
			ep0_in_xfer((const uint8_t *)&cdc_line_coding, sizeof(cdc_line_coding_t));
			break;

		case CDC_REQ_SET_LINE_CODING:
			/* Prepare EP0 OUT to receive the 7-byte line coding */
			ep0_out_prime(sizeof(cdc_line_coding_t));
			break;

		case CDC_REQ_SET_CONTROL_LINE_STATE:
			/* Host sets DTR/RTS (bit 0 = DTR) */
			usb_connected = (pkt->wValue & 1) ? 1 : 0;
			/* Acknowledge with zero-length packet */
			ep0_in_xfer(0, 0);
			break;

		default:
			ep0_in_xfer(0, 0);
			break;
		}
	} else {
		ep0_in_xfer(0, 0);
	}
}

void usb_init(void)
{
	/* 1. De-assert reset on USBCTRL */
	mmio32(RESETS_RESET) &= ~RESETS_USBCTRL_BIT;
	while ((mmio32(RESETS_RESET_DONE) & RESETS_USBCTRL_BIT) == 0);

	/* 2. Clear entire 4KB USB DPRAM */
	memset((void *)USB_DPRAM_BASE, 0, 0x1000);

	/* 3. Mux controller to onboard USB PHY */
	mmio32(USB_MUXING) = (1 << 0) | (1 << 3); /* TO_PHY | SOFTCON */

	/* 4. Force VBUS detect so device knows it is connected */
	mmio32(USB_PWR) = (1 << 2) | (1 << 3);    /* VBUS_DETECT | OVERRIDE_EN */

	/* 5. Enable controller in device mode */
	mmio32(USB_MAIN_CTRL) = (1 << 0);         /* CONTROLLER_EN */

	/* 6. Enable an interrupt per EP0 transaction (used by BUFF_STATUS) */
	mmio32(USB_SIE_CTRL) = (1 << 29);         /* EP0_INT_1BUF */

	/* 7. Connect pull-up resistor to D+ (PULLUP_EN) to signal device presence to PC */
	mmio32(USB_SIE_CTRL + REG_ALIAS_SET) = (1 << 16); /* PULLUP_EN */
}

void usb_poll(void)
{
	uint32_t sie_status = mmio32(USB_SIE_STATUS);

	/* 1. Check for USB Bus Reset from host */
	if (sie_status & (1 << 19)) { /* BUS_RESET bit */
		/* Atomically clear bit via RP2040 CLR alias */
		mmio32(USB_SIE_STATUS + REG_ALIAS_CLR) = (1 << 19);
		usb_dev_addr = 0;
		usb_pending_addr = 0;
		usb_configured = 0;
		usb_connected = 0;
		ep0_in_remain = 0;
		usb_rx_head = 0;
		usb_rx_tail = 0;
		mmio32(USB_ADDR_ENDP) = 0;
	}

	/* 2. Check for SETUP packet */
	if (sie_status & (1 << 17)) { /* SETUP_REC bit */
		/* Atomically clear bit via RP2040 CLR alias */
		mmio32(USB_SIE_STATUS + REG_ALIAS_CLR) = (1 << 17);
		handle_setup_packet();
	}

	/* 3. Check buffer completion status */
	uint32_t buf_status = mmio32(USB_BUFF_STATUS);
	if (buf_status) {
		/* Atomically clear completed buffer flags via RP2040 CLR alias */
		mmio32(USB_BUFF_STATUS + REG_ALIAS_CLR) = buf_status;

		/* EP0 IN completed */
		if (buf_status & (1 << 0)) {
			if (ep0_in_remain > 0) {
				/* Continue multi-packet transfer (e.g. 75-byte Config Descriptor) */
				ep0_in_send_chunk();
			} else if (usb_pending_addr != 0) {
				/* SET_ADDRESS: Status handshake complete on bus.
				 * Set hardware address now! */
				usb_dev_addr = usb_pending_addr;
				mmio32(USB_ADDR_ENDP) = usb_dev_addr;
				usb_pending_addr = 0;
			} else if (ep0_dir_in) {
				/* Control Read data stage complete:
				 * Prime EP0 OUT for host's zero-length status handshake packet */
				ep0_out_prime(0);
			}
			/* Note: For Control Write (ep0_dir_in == 0), the ZLP on EP0 IN was
			 * the status stage; no OUT status packet exists, transfer is finished. */
		}

		/* EP0 OUT completed (e.g. SET_LINE_CODING data received or status handshake) */
		if (buf_status & (1 << 1)) {
			if (usb_configured && !ep0_dir_in) {
				memcpy(&cdc_line_coding, (void *)DPRAM_EP0_BUF, sizeof(cdc_line_coding_t));
				ep0_in_xfer(0, 0);
			}
		}

		/* EP2 OUT completed (Host sent data to Pico) */
		if (buf_status & (1 << 5)) {
			uint32_t ctrl = mmio32(DPRAM_EP2_OUT_BUF_CTRL);
			uint32_t count = ctrl & USB_BUF_CTRL_LEN_MASK;
			const uint8_t *src = (const uint8_t *)DPRAM_EP2_OUT_BUF;

			for (uint32_t i = 0; i < count; i++) {
				uint16_t next = (usb_rx_head + 1) % USB_RX_BUF_SIZE;
				if (next != usb_rx_tail) {
					usb_rx_buf[usb_rx_head] = src[i];
					usb_rx_head = next;
				}
			}

			/* Clear bit 5 in BUFF_STATUS */
			mmio32(USB_BUFF_STATUS) = (1 << 5);

			/* Re-prime EP2 OUT for next packet */
			ep2_out_pid ^= 1;
			uint32_t next_ctrl = (ep2_out_pid ? USB_BUF_CTRL_DATA_PID : 0) |
			                     USB_BUF_CTRL_AVAIL | 64;
			mmio32(DPRAM_EP2_OUT_BUF_CTRL) = next_ctrl;
		}
	}
}

int usb_cdc_is_connected(void)
{
	return (usb_configured && usb_connected);
}

int usb_cdc_write(const uint8_t *data, uint32_t len)
{
	if (!usb_configured) return 0;

	/* If previous transfer is still in progress, wait or return */
	if (mmio32(DPRAM_EP2_IN_BUF_CTRL) & USB_BUF_CTRL_AVAIL) {
		return 0; /* Busy */
	}

	if (len > 64) len = 64;
	memcpy((void *)DPRAM_EP2_IN_BUF, data, len);

	uint32_t ctrl = USB_BUF_CTRL_FULL |
	                (ep2_in_pid ? USB_BUF_CTRL_DATA_PID : 0) |
	                USB_BUF_CTRL_AVAIL |
	                (len & USB_BUF_CTRL_LEN_MASK);

	mmio32(DPRAM_EP2_IN_BUF_CTRL) = ctrl;
	ep2_in_pid ^= 1;
	return len;
}

int usb_cdc_read(uint8_t *data, uint32_t max_len)
{
	if (!usb_configured || !data || max_len == 0) return 0;

	uint32_t read_bytes = 0;
	while (usb_rx_head != usb_rx_tail && read_bytes < max_len) {
		data[read_bytes++] = usb_rx_buf[usb_rx_tail];
		usb_rx_tail = (usb_rx_tail + 1) % USB_RX_BUF_SIZE;
	}
	return read_bytes;
}
