/*
 * Copyright (c) 2023 Florin9doi
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "migration/vmstate.h"
#include "qapi/error.h"

typedef struct XboxDVDPlaybackKitState {
    USBDevice dev;
    char *firmware_path;
    uint32_t firmware_len;
    uint8_t firmware[0x40000];
} XboxDVDPlaybackKitState;

enum {
    STR_EMPTY,
    STR_MANUFACTURER,
    STR_PRODUCT,
};

static const USBDescIface desc_iface[] = {
    //*
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = 0x58, // USB_CLASS_XID,
        .bInterfaceSubClass = 0x42, // USB_DT_XID
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress = USB_DIR_IN | 0x01,
                .bmAttributes     = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize   = 8,
                .bInterval        = 16,
            },
        },
    },
    {
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 0,
        .bInterfaceClass    = 0x59,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
    },
    /*/
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 0,
        .bInterfaceClass    = 0x59,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
    },
    //*/
};

static const USBDescDevice desc_device = {
    .bcdUSB             = 0x0110,
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = 64,
    .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces      = 2,
            //.bNumInterfaces      = 1,
            .bConfigurationValue = 1,
            .iConfiguration      = STR_EMPTY,
            .bmAttributes        = 0x00,
            .bMaxPower           = 0x00,
            .nif = ARRAY_SIZE(desc_iface),
            .ifs = desc_iface,
        },
    },
};

static const USBDesc desc_xbox_dvd_playback_kit = {
    .id = {
        .idVendor  = 0x045e,
        .idProduct = 0x0284,
        .bcdDevice = 0x0100,
        .iManufacturer = STR_EMPTY,
        .iProduct      = STR_EMPTY,
        .iSerialNumber = STR_EMPTY,
    },
    .full = &desc_device,
};

static void xbox_dvd_playback_kit_realize(USBDevice *dev, Error **errp) {
    XboxDVDPlaybackKitState *s = (XboxDVDPlaybackKitState *) dev;

    usb_desc_init(dev);
    if (!s->firmware_path) {
        error_setg(errp, "Firmware file is required");
        return;
    }
    int fd = open(s->firmware_path, O_RDONLY | O_BINARY);
    if (fd < 0) {
        error_setg(errp, "Unable to access \"%s\"", s->firmware_path);
        return;
    }
    size_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    s->firmware_len = read(fd, s->firmware, size);
    close(fd);
}

static void xbox_dvd_playback_kit_handle_control(USBDevice *dev, USBPacket *p,
        int request, int value, int index, int length, uint8_t *data) {
    XboxDVDPlaybackKitState *s = (XboxDVDPlaybackKitState *) dev;
    int ret;

    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        //*
        fprintf(stderr, "xbox_dvd_playback_kit_handle_control : "
            "req=0x%x val=0x%x idx=0x%x len=0x%x ret=%d / data=",
            request, value, index, length, ret);
        for (int i = 0; i < length; i++) {
            fprintf(stderr, "%02x ", data[i]);
        }
        fprintf(stderr, "\n");
        //*/
        return;
    }

    switch (request) {
    case 0xc101:
    case 0xc102:
        {
            uint32_t offset = 0x400 * value;
            if (offset + length <= s->firmware_len) {
                memcpy(data, s->firmware + offset, length);
                p->actual_length = length;
            } else {
                p->actual_length = 0;
            }
        }
        break;
    case 0xc106: // GET_DESCRIPTOR
        data[0] = 0x08; // len
        data[1] = 0x42; // type
        data[2] = 0x00; // xid
        data[3] = 0x01; // xid
        data[4] = 0x03; // type
        data[5] = 0x00; // subtype
        data[6] = 0x06; // in size
        data[7] = 0x00; // out size
        p->actual_length = 8;
        break;
    case 0xa101: // GET_REPORT
        p->status = USB_RET_STALL;
        data[0] = 0xff;
        data[1] = 0xff;
        data[2] = 0xff;
        data[3] = 0xff;
        data[4] = 0xff;
        data[5] = 0xff;
        p->actual_length = 0;
        break;
    }

    //*
    fprintf(stderr, "xbox_dvd_playback_kit_handle_control : "
        "req=0x%02x val=0x%02x idx=0x%02x len=0x%02x / data=",
        request, value, index, length);
    for (int i = 0; i < length && i < p->actual_length && i < 32; i++) {
        fprintf(stderr, "%02x ", data[i]);
    }
    fprintf(stderr, "\n");
    //*/
}

static void xbox_dvd_playback_kit_handle_data(USBDevice *dev, USBPacket *p) {
    switch(p->pid) {
        case USB_TOKEN_OUT:
            fprintf(stderr, "xbox_dvd_playback_kit_handle_data OUT : ep=%d\n", p->ep->nr);
            break;
        case USB_TOKEN_IN:
            fprintf(stderr, "xbox_dvd_playback_kit_handle_data IN : ep=%d, sz=%lu\n", p->ep->nr, p->iov.size);
            {
            uint8_t data[] = {0x00, 0x06, 0xa7, 0x0A, 0x40, 0x00};
            usb_packet_copy(p, data, sizeof(data));
            }
            break;
    }
}

static void xbox_dvd_playback_kit_class_init(ObjectClass *klass, void *class_data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Microsoft Xbox DVD Playback Kit";
    uc->usb_desc       = &desc_xbox_dvd_playback_kit;
    uc->realize        = xbox_dvd_playback_kit_realize;
    uc->handle_control = xbox_dvd_playback_kit_handle_control;
    uc->handle_data    = xbox_dvd_playback_kit_handle_data;

    dc->desc = "Microsoft Xbox DVD Playback Kit";
}

static char *xbox_dvd_playback_kit_get_firmware(Object *obj, Error **errp)
{
    XboxDVDPlaybackKitState *s = (XboxDVDPlaybackKitState *) obj;
    return g_strdup(s->firmware_path);
}

static void xbox_dvd_playback_kit_set_firmware(Object *obj, const char *value, Error **errp) {
    XboxDVDPlaybackKitState *s = (XboxDVDPlaybackKitState *) obj;

    if (access(value, F_OK) != 0) {
        error_setg(errp, "Unable to access \"%s\"", value);
        return;
    }
    g_free(s->firmware_path);
    s->firmware_path = g_strdup(value);
}

static void xbox_dvd_playback_kit_instance_init(Object *obj) {
    object_property_add_str(obj, "file",
                            xbox_dvd_playback_kit_get_firmware,
                            xbox_dvd_playback_kit_set_firmware);
    object_property_set_description(obj, "file",
                                    "Set the Xbox DVD Playback Kit firmware");
}

static const TypeInfo xbox_dvd_playback_kit_info = {
    .name          = "xbox_dvd_playback_kit",
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(XboxDVDPlaybackKitState),
    .class_init    = xbox_dvd_playback_kit_class_init,
    .instance_init = xbox_dvd_playback_kit_instance_init,
};

static void usb_xbox_dvd_playback_kit_register_types(void) {
    type_register_static(&xbox_dvd_playback_kit_info);
}

type_init(usb_xbox_dvd_playback_kit_register_types)
