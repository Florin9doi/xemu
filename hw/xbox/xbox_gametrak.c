/*
 * Copyright (c) 2025 Florin9doi
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

#include "xid.h"

typedef struct XboxGametrakReport {
    uint8_t bReportId;
    uint8_t bLength;
    uint16_t wButton;
    uint16_t wTimer;
} QEMU_PACKED XboxGametrakReport;

typedef struct XboxGametrakState {
    USBDevice dev;
    uint8_t device_index;
    XboxGametrakReport in_state;
} XboxGametrakState;

enum {
    STR_EMPTY
};

static const USBDescIface desc_iface[] = {
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
            .bConfigurationValue = 1,
            .iConfiguration      = STR_EMPTY,
            .bmAttributes        = 0x00,
            .bMaxPower           = 0x00,
            .nif = ARRAY_SIZE(desc_iface),
            .ifs = desc_iface,
        },
    },
};

static const USBDesc desc_xbox_gametrak = {
    .id = {
        .idVendor      = 0x045e,
        .idProduct     = 0x0284,
        .bcdDevice     = 0x0100,
        .iManufacturer = STR_EMPTY,
        .iProduct      = STR_EMPTY,
        .iSerialNumber = STR_EMPTY,
    },
    .full = &desc_device,
};

static const XIDDesc desc_xid_xbox_gametrak = {
    .bLength              = 0x08,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x0100,
    .bType                = XID_DEVICETYPE_DVD_PLAYBACK_KIT,
    .bSubType             = XID_DEVICESUBTYPE_DVD_PLAYBACK_KIT,
    .bMaxInputReportSize  = 0x06,
    .bMaxOutputReportSize = 0x00,
};

static void xbox_gametrak_realize(USBDevice *dev, Error **errp) {
    XboxGametrakState *s = (XboxGametrakState *) dev;

    usb_desc_init(dev);
}

static void xbox_gametrak_handle_control(USBDevice *dev, USBPacket *p,
        int request, int value, int index, int length, uint8_t *data) {
    XboxGametrakState *s = (XboxGametrakState *) dev;

    int ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }

    switch (request) {
    case 0xc101:
    case 0xc102:
    {
        break;
    }
    case 0xc106: // GET_DESCRIPTOR
        memcpy(data, &desc_xid_xbox_gametrak, desc_xid_xbox_gametrak.bLength);
        p->actual_length = desc_xid_xbox_gametrak.bLength;
        break;
    case 0xa101: // GET_REPORT
    default:
        p->actual_length = 0;
        p->status = USB_RET_STALL;
        break;
    }
}

static void update_dvd_kit_input(XboxGametrakState *s)
{
    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    xemu_input_update_controller(state);

    s->in_state.bReportId = 0x00;
    s->in_state.bLength = 0x06;
    s->in_state.wButton = 0x0000;
    if (state->dvdKit.buttons) {
        // for (int i = 0; i < sizeof(dvd_button_ids) / sizeof(dvd_button_ids[0]); i++) {
        //     if ((1ULL << i) & state->dvdKit.buttons) {
        //         s->in_state.wButton = dvd_button_ids[i].id;
        //         return;
        //     }
        // }
    }
}

static void xbox_gametrak_handle_data(USBDevice *dev, USBPacket *p) {
    XboxGametrakState *s = DO_UPCAST(XboxGametrakState, dev, dev);

    switch (p->pid) {
        case USB_TOKEN_IN:
            update_dvd_kit_input(s);
            usb_packet_copy(p, &s->in_state, s->in_state.bLength);
            break;
        case USB_TOKEN_OUT:
        default:
            break;
    }
}

static Property xid_properties[] = {
    DEFINE_PROP_UINT8("index", XboxGametrakState, device_index, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void xbox_gametrak_class_init(ObjectClass *klass, void *class_data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Xbox Gametrak";
    uc->usb_desc       = &desc_xbox_gametrak;
    uc->realize        = xbox_gametrak_realize;
    uc->handle_control = xbox_gametrak_handle_control;
    uc->handle_data    = xbox_gametrak_handle_data;

    device_class_set_props(dc, xid_properties);
    dc->desc = "Xbox Gametrak";
}

static const TypeInfo xbox_gametrak_info = {
    .name          = TYPE_USB_XID_GAMETRAK,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(XboxGametrakState),
    .class_init    = xbox_gametrak_class_init,
};

static void usb_xbox_gametrak_register_types(void) {
    type_register_static(&xbox_gametrak_info);
}

type_init(usb_xbox_gametrak_register_types)
