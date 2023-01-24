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
#include "xbox_camera_jo_jpeg.h"

typedef struct XboxCameraState {
    USBDevice dev;
    uint8_t controller[0x100];
    uint8_t sensor[0x100];
    int state;
    int offset;
} XboxCameraState;

enum {
    STR_EMPTY,
    STR_MANUFACTURER,
    STR_PRODUCT,
};

static const uint8_t ov519_defaults[] = {
    0xc0, 0x00, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x98, 0xff, 0x00, 0x03, 0x00, 0x00, 0x1e, 0x01, 0xf1, 0x00, 0x01, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0x50, 0x51, 0x52, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x01, 0x00, 0x21, 0x00, 0x02, 0x6d, 0x0e, 0x00, 0x02, 0x00, 0x11,
    0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb4, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x03, 0x03, 0xfc, 0x00, 0xff, 0x00, 0x00, 0xff,
    0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x04, 0x40, 0x40, 0x0c, 0x3f, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0a, 0x0f, 0x1e, 0x2d, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x00, 0x05, 0x02, 0x07, 0x00, 0x09, 0x02, 0x0b, 0x00, 0x0d, 0x02, 0x0f,
    0x00, 0x11, 0x02, 0x13, 0x00, 0x15, 0x02, 0x17, 0x00, 0x19, 0x02, 0x1b, 0x00, 0x1d, 0x02, 0x1f,
    0x50, 0x64, 0x82, 0x96, 0x82, 0x81, 0x00, 0x01, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t ov7648_defaults[] = {
    0x00, 0x84, 0x84, 0x84, 0x34, 0x3e, 0x80, 0x8c, 0x00, 0x00, 0x76, 0x48, 0x7b, 0x5b, 0x00, 0x98,
    0x57, 0x00, 0x14, 0xa3, 0x04, 0x00, 0x00, 0x1a, 0xba, 0x03, 0xf3, 0x00, 0x7f, 0xa2, 0x00, 0x01,
    0xc0, 0x80, 0x80, 0xde, 0x10, 0x8a, 0xa2, 0xe2, 0x20, 0x00, 0x00, 0x00, 0x88, 0x81, 0x00, 0x94,
    0x40, 0xa0, 0xc0, 0x16, 0x16, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca, 0x00,
    0x06, 0xe0, 0x88, 0x11, 0x89, 0x02, 0x55, 0x01, 0x7a, 0x04, 0x00, 0x00, 0x11, 0x01, 0x06, 0x00,
    0x01, 0x00, 0x10, 0x50, 0x20, 0x02, 0x00, 0xf3, 0x80, 0x80, 0x80, 0x00, 0x00, 0x47, 0x27, 0x8a,
    0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x82, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x75, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const USBDescIface desc_iface[] = {
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress = USB_DIR_IN | 0x01,
                .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
                .wMaxPacketSize   = 0,
                .bInterval        = 1,
            },
        },
    },
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 1,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress = USB_DIR_IN | 0x01,
                .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
                .wMaxPacketSize   = 384,
                .bInterval        = 1,
            },
        },
    },
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 2,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress = USB_DIR_IN | 0x01,
                .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
                .wMaxPacketSize   = 512,
                .bInterval        = 1,
            },
        },
    },
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 3,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress = USB_DIR_IN | 0x01,
                .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
                .wMaxPacketSize   = 768,
                .bInterval        = 1,
            },
        },
    },
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 4,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress = USB_DIR_IN | 0x01,
                .bmAttributes     = USB_ENDPOINT_XFER_ISOC,
                .wMaxPacketSize   = 896,
                .bInterval        = 1,
            },
        },
    },

    /*
    {
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 0,
        .bInterfaceClass    = USB_CLASS_AUDIO,
        .bInterfaceSubClass = USB_SUBCLASS_AUDIO_CONTROL,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .ndesc              = 3,
        .descs = (USBDescOther[]) {
            {
                .data = (uint8_t[]) {
                    0x09,
                    USB_DT_CS_INTERFACE,
                    0x01,
                    0x00, 0x01,
                    0x1e, 0x00,
                    0x01,
                    0x02,
                }
            },
            {
                .data = (uint8_t[]) {
                    0x0c,
                    USB_DT_CS_INTERFACE,
                    0x02,
                    0x01,
                    0x01, 0x02,
                    0x00,
                    0x01,
                    0x00, 0x00,
                    0x00,
                    0x00,
                }
            },
            {
                .data = (uint8_t[]) {
                    0x09,
                    USB_DT_CS_INTERFACE,
                    0x03,
                    0x02,
                    0x01, 0x01,
                    0x00,
                    0x01,
                    0x00,
                }
            }
        },
    },
    {
        .bInterfaceNumber   = 2,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 0,
        .bInterfaceClass    = USB_CLASS_AUDIO,
        .bInterfaceSubClass = USB_SUBCLASS_AUDIO_STREAMING,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
    },
    {
        .bInterfaceNumber   = 2,
        .bAlternateSetting  = 1,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_AUDIO,
        .bInterfaceSubClass = USB_SUBCLASS_AUDIO_STREAMING,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .ndesc              = 2,
        .descs = (USBDescOther[]) {
            {
                .data = (uint8_t[]) {
                    0x07,
                    USB_DT_CS_INTERFACE,
                    0x01,
                    0x02,
                    0x01,
                    0x01, 0x00,
                }
            },
            {
                .data = (uint8_t[]) {
                    0x0b,
                    USB_DT_CS_INTERFACE,
                    0x02,
                    0x01,
                    0x01,
                    0x02,
                    0x10,
                    0x01,
                    0x80, 0x3e, 0x00,
                }
            }
        },
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | 0x02,
                .bmAttributes          = 0x05,
                .wMaxPacketSize        = 0x28,
                .bInterval             = 1,
                .is_audio              = 1,
                .extra = (uint8_t[]) {
                    0x07,
                    USB_DT_CS_ENDPOINT,
                    0x01,
                    0x00,
                    0x00,
                    0x00, 0x00,
                },
            },
        },
    }
    //*/
};

static const USBDescDevice desc_device = {
    .bcdUSB             = 0x0110,
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = 8,
    .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces      = 1,
            .bConfigurationValue = 1,
            .iConfiguration      = STR_EMPTY,
            .bmAttributes        = 0x80,
            .bMaxPower           = 0xfa,
            .nif = ARRAY_SIZE(desc_iface),
            .ifs = desc_iface,
        },
    },
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER] = "Microsoft",
    [STR_PRODUCT] = "Xbox Video Camera",
};

static const USBDesc desc_xbox_camera = {
    .id = {
        .idVendor  = 0x045e,
        .idProduct = 0x028c,
        .bcdDevice = 0x0100,
        .iManufacturer = STR_MANUFACTURER,
        .iProduct      = STR_PRODUCT,
        .iSerialNumber = STR_EMPTY,
    },
    .full = &desc_device,
    .str  = desc_strings,
};

static void xbox_camera_realize(USBDevice *dev, Error **errp) {
    usb_desc_init(dev);
}

static void xbox_camera_handle_reset(USBDevice *dev) {
    XboxCameraState *s = (XboxCameraState *) dev;

    fprintf(stderr, "xbox_camera_handle_reset\n");
    memcpy(s->controller, ov519_defaults, sizeof(ov519_defaults));
    memcpy(s->sensor, ov7648_defaults, sizeof(ov7648_defaults));
    s->state = 0;
    s->offset = 0;
}

static void xbox_camera_handle_control(USBDevice *dev, USBPacket *p,
        int request, int value, int index, int length, uint8_t *data) {
    XboxCameraState *s = (XboxCameraState *) dev;
    int ret;

    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        /*
        fprintf(stderr, "xbox_camera_handle_control : "
            "req=0x%x val=0x%x idx=0x%x len=0x%x ret=%d / data=",
            request, value, index, length, ret);
        for (int i = 0; i < length; i++) {
            fprintf(stderr, "%02x ", data[i]);
        }
        fprintf(stderr, "\n");
        return;
        //*/
    }

    switch (request) {
    case 0x4101: // write
        if (index == 0x47) {
            if (data[0] == 1) {
                //fprintf(stderr, " - write to reg 0x%02x value 0x%02x\n", s->controller[0x42], s->controller[0x45]);
                s->sensor[s->controller[0x42]] = s->controller[0x45];
            } else if (data[0] == 5) {
                //fprintf(stderr, " - read from reg 0x%02x value 0x%02x\n", s->controller[0x43], s->sensor[s->controller[0x43]]);
                s->controller[0x45] = s->sensor[s->controller[0x43]];
            }
        }
        s->controller[index & 0xff] = data[0];
        p->actual_length = 1;
        break;
    case 0xc101: // read
        for (int i = 0; i < length; i++) {
            data[i] = s->controller[index & 0xff];
        }
        p->actual_length = length;
        break;
    }

    switch (request) {
    case 0x4101: // write
        // if (index == 0x42 || index == 0x43 || index == 0x45) {
        //     fprintf(stderr, "    X reg_write(dev_handle, 0x%02x, 0x%02x);\n", index, data[0]);
        // } else
        // if (index == 0x47) {
        //     if (data[0] == 1) {
        //         fprintf(stderr, "        i2c_write(dev_handle, 0x%02x, 0x%02x);\n", s->controller[0x42], s->controller[0x45]);
        //     } else if (data[0] == 5) {
        //         fprintf(stderr, "        i2c_read(dev_handle, 0x%02x);\n", s->controller[0x43]);
        //     } else {
        //         fprintf(stderr, "    X reg_write(dev_handle, 0x%02x, 0x%02x);\n", index, data[0]);
        //     }
        // } else {
            fprintf(stderr, "    reg_write(dev_handle, 0x%02x, 0x%02x);\n", index, data[0]);
        // }
        break;
    case 0xc101: // read
        // if (index == 0x47) {
        //     fprintf(stderr, "    X reg_read(dev_handle, 0x%02x);\n", index);
        // } else {
            fprintf(stderr, "    reg_read(dev_handle, 0x%02x);\n", index);
        // }
        break;
    }


    /*
    fprintf(stderr, "xbox_camera_handle_control : "
        "req=0x%02x val=0x%02x idx=0x%02x len=0x%02x / data=",
        request, value, index, length);
    for (int i = 0; i < length; i++) {
        fprintf(stderr, "%02x ", data[i]);
    }
    fprintf(stderr, "\n");
    //*/
}

static uint8_t *jpgData = NULL;
static uint32_t jpgSize = 0;
static uint8_t frameId = 0;
uint8_t *readJpgData(void);
/*
uint8_t *readJpgData(void) {
    int fd = open("image_00_jojpeg.jpg", O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    rawDataSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    uint8_t *buf = malloc(rawDataSize);
    rawDataSize = read(fd, buf, rawDataSize);
    close(fd);
    return buf;
}
/*/
uint8_t *readJpgData(void) {
    int width = 320;
    int height = 240;
    unsigned char rawData[3 * width * height];
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            unsigned char *p = rawData + 3 * (width * y + x);
            p[0] = 255 * x / width + frameId;
            p[1] = 255 * y / height;
            p[2] = 0x00;
        }
    }
    frameId = frameId < 255 ? frameId+1 : 0;

    if (jpgData == NULL) {
        jpgData = malloc(width * height * 2);
    }
    uint8_t *x = jpgData;
    jpgSize = jo_write_jpg(&x, rawData, width, height, 3, 44);
    return jpgData;
}
//*/

static void xbox_camera_handle_data(USBDevice *dev, USBPacket *p) {
    XboxCameraState *s = (XboxCameraState *) dev;
    uint16_t max_size = 768;
    uint8_t pk[max_size];
    //memset(pk, 0xff, max_size);

    switch(p->pid) {
        case USB_TOKEN_OUT:
            fprintf(stderr, "xbox_camera_handle_data OUT : ep=%d\n", p->ep->nr);
            break;
        case USB_TOKEN_IN:
        {
            //fprintf(stderr, "xbox_camera_handle_data IN : ep=%d, sz=%lu, stat=%d\n", p->ep->nr, p->iov.size, s->state);

            //*
            if (s->state == 0) {
            //fprintf(stderr, "xbox_camera_handle_data IN : ep=%d, sz=%lu, stat=%d\n", p->ep->nr, p->iov.size, s->state);
                readJpgData();
                uint8_t header[] = {0xFF, 0xFF, 0xFF, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
                memcpy(pk, header, sizeof(header));
                int data_pk = max_size - sizeof(header);
                memcpy(pk + sizeof(header), jpgData, data_pk);
                s->offset = data_pk;
                s->state++;

                usb_packet_copy(p, pk, max_size);
            } else if (s->offset < jpgSize) {
                int data_pk = jpgSize - s->offset;
                if (data_pk > max_size)
                    data_pk = max_size;
                memcpy(pk, jpgData + s->offset, data_pk);
                s->offset += data_pk;
                s->state++;

                usb_packet_copy(p, pk, max_size);
            } else {
                if (s->state == 30) {
                    s->state = 0;
                    p->status = USB_RET_NAK;
                    p->actual_length = 0;
                } else if (s->state >= 25) {
                    s->state++;
                    p->status = USB_RET_NAK;
                    p->actual_length = 0;
                } else {
                    uint8_t footer[] = {0xFF, 0xFF, 0xFF, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
                    memcpy(pk, footer, sizeof(footer));
                    s->state = 25;

                    usb_packet_copy(p, pk, max_size);
                }
            }
            break;
        }
    }
}

// static const VMStateDescription xbox_camera_vmstate = {
//     .name = "xbox_camera",
//     .version_id = 1,
// };

static void xbox_camera_class_init(ObjectClass *klass, void *class_data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Microsoft Xbox Camera";
    uc->usb_desc       = &desc_xbox_camera;
    uc->realize        = xbox_camera_realize;
    uc->handle_reset   = xbox_camera_handle_reset;
    uc->handle_control = xbox_camera_handle_control;
    uc->handle_data    = xbox_camera_handle_data;

    dc->desc = "Microsoft Xbox Camera";
    //dc->vmsd = &xbox_camera_vmstate;
}

static const TypeInfo xbox_camera_info = {
    .name          = "xbox_camera",
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(XboxCameraState),
    .class_init    = xbox_camera_class_init,
};

static void usb_xbox_camera_register_types(void) {
    type_register_static(&xbox_camera_info);
}

type_init(usb_xbox_camera_register_types)
