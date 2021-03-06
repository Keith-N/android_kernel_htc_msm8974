/*
 * GeneSys GL620USB-A based links
 * Copyright (C) 2001 by Jiun-Jie Huang <huangjj@genesyslogic.com.tw>
 * Copyright (C) 2001 by Stanislav Brabec <utx@penguin.cz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>
#include <linux/gfp.h>



#define GENELINK_CONNECT_WRITE			0xF0
#define GENELINK_INTERRUPT_PIPE			0x03
#define INTERRUPT_BUFSIZE			0x08
#define GENELINK_INTERRUPT_INTERVAL		0x10
#define GL_MAX_TRANSMIT_PACKETS			32
#define GL_MAX_PACKET_LEN			1514
#define GL_RCV_BUF_SIZE		\
	(((GL_MAX_PACKET_LEN + 4) * GL_MAX_TRANSMIT_PACKETS) + 4)

struct gl_packet {
	__le32		packet_length;
	char		packet_data [1];
};

struct gl_header {
	__le32			packet_count;
	struct gl_packet	packets;
};

static int genelink_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	struct gl_header	*header;
	struct gl_packet	*packet;
	struct sk_buff		*gl_skb;
	u32			size;
	u32			count;

	header = (struct gl_header *) skb->data;

	
	count = le32_to_cpu(header->packet_count);
	if (count > GL_MAX_TRANSMIT_PACKETS) {
		dbg("genelink: invalid received packet count %u", count);
		return 0;
	}

	
	packet = &header->packets;

	
	skb_pull(skb, 4);

	while (count > 1) {
		
		size = le32_to_cpu(packet->packet_length);

		
		if (size > GL_MAX_PACKET_LEN) {
			dbg("genelink: invalid rx length %d", size);
			return 0;
		}

		
		gl_skb = alloc_skb(size, GFP_ATOMIC);
		if (gl_skb) {

			
			memcpy(skb_put(gl_skb, size),
					packet->packet_data, size);
			usbnet_skb_return(dev, gl_skb);
		}

		
		packet = (struct gl_packet *)&packet->packet_data[size];
		count--;

		
		skb_pull(skb, size + 4);
	}

	
	skb_pull(skb, 4);

	if (skb->len > GL_MAX_PACKET_LEN) {
		dbg("genelink: invalid rx length %d", skb->len);
		return 0;
	}
	return 1;
}

static struct sk_buff *
genelink_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags)
{
	int 	padlen;
	int	length = skb->len;
	int	headroom = skb_headroom(skb);
	int	tailroom = skb_tailroom(skb);
	__le32	*packet_count;
	__le32	*packet_len;

	
	padlen = ((skb->len + (4 + 4*1)) % 64) ? 0 : 1;

	if ((!skb_cloned(skb))
			&& ((headroom + tailroom) >= (padlen + (4 + 4*1)))) {
		if ((headroom < (4 + 4*1)) || (tailroom < padlen)) {
			skb->data = memmove(skb->head + (4 + 4*1),
					     skb->data, skb->len);
			skb_set_tail_pointer(skb, skb->len);
		}
	} else {
		struct sk_buff	*skb2;
		skb2 = skb_copy_expand(skb, (4 + 4*1) , padlen, flags);
		dev_kfree_skb_any(skb);
		skb = skb2;
		if (!skb)
			return NULL;
	}

	
	packet_count = (__le32 *) skb_push(skb, (4 + 4*1));
	packet_len = packet_count + 1;

	*packet_count = cpu_to_le32(1);
	*packet_len = cpu_to_le32(length);

	
	if ((skb->len % dev->maxpacket) == 0)
		skb_put(skb, 1);

	return skb;
}

static int genelink_bind(struct usbnet *dev, struct usb_interface *intf)
{
	dev->hard_mtu = GL_RCV_BUF_SIZE;
	dev->net->hard_header_len += 4;
	dev->in = usb_rcvbulkpipe(dev->udev, dev->driver_info->in);
	dev->out = usb_sndbulkpipe(dev->udev, dev->driver_info->out);
	return 0;
}

static const struct driver_info	genelink_info = {
	.description =	"Genesys GeneLink",
	.flags =	FLAG_POINTTOPOINT | FLAG_FRAMING_GL | FLAG_NO_SETINT,
	.bind =		genelink_bind,
	.rx_fixup =	genelink_rx_fixup,
	.tx_fixup =	genelink_tx_fixup,

	.in = 1, .out = 2,

#ifdef	GENELINK_ACK
	.check_connect =genelink_check_connect,
#endif
};

static const struct usb_device_id	products [] = {

{
	USB_DEVICE(0x05e3, 0x0502),	
	.driver_info =	(unsigned long) &genelink_info,
},
	{ },		
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver gl620a_driver = {
	.name =		"gl620a",
	.id_table =	products,
	.probe =	usbnet_probe,
	.disconnect =	usbnet_disconnect,
	.suspend =	usbnet_suspend,
	.resume =	usbnet_resume,
};

module_usb_driver(gl620a_driver);

MODULE_AUTHOR("Jiun-Jie Huang");
MODULE_DESCRIPTION("GL620-USB-A Host-to-Host Link cables");
MODULE_LICENSE("GPL");

