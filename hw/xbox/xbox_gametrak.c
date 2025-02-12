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

static const USBDescIface desc_iface_xbox_gametrak = {
    .bInterfaceNumber   = 0x00,
    .bNumEndpoints      = 0x02,
    .bInterfaceClass    = USB_CLASS_XID,
    .bInterfaceSubClass = USB_DT_XID,
    .bInterfaceProtocol = 0x00,
    .eps =
        (USBDescEndpoint[]){
            {
                .bEndpointAddress = USB_DIR_IN | 0x02,
                .bmAttributes     = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize   = 0x20,
                .bInterval        = 4,
            },
            {
                .bEndpointAddress = USB_DIR_OUT | 0x02,
                .bmAttributes     = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize   = 0x20,
                .bInterval        = 4,
            },
        },
};

static const USBDescDevice desc_device_xbox_gametrak = {
    .bcdUSB = 0x0110,
    .bMaxPacketSize0 = 0x40,
    .bNumConfigurations = 1,
    .confs =
        (USBDescConfig[]){
            {
                .bNumInterfaces      = 1,
                .bConfigurationValue = 1,
                .bmAttributes        = USB_CFG_ATT_ONE,
                .bMaxPower           = 50,
                .nif                 = 1,
                .ifs                 = &desc_iface_xbox_gametrak,
            },
        },
};

static const USBDesc desc_xbox_gametrak = {
    .id = {
        .idVendor      = 0x045e,
        .idProduct     = 0x0202,
        .bcdDevice     = 0x0100,
        .iManufacturer = STR_MANUFACTURER,
        .iProduct      = STR_PRODUCT,
        .iSerialNumber = STR_SERIALNUMBER,
    },
    .full = &desc_device_xbox_gametrak,
    .str  = desc_strings,
};

static const XIDDesc desc_xid_xbox_gametrak = {
    .bLength              = 0x10,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x0100,
    .bType                = XID_DEVICETYPE_GAMEPAD,
    .bSubType             = XID_DEVICESUBTYPE_GAMEPAD,
    .bMaxInputReportSize  = 20,
    .bMaxOutputReportSize = 6,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

static void xbox_gametrak_realize(USBDevice *dev, Error **errp) {
    USBXIDGamepadState *s = (USBXIDGamepadState *) dev;

    usb_desc_init(dev);
    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.length = sizeof(s->out_state);
    s->out_state.report_id = 0;

    s->xid_desc = &desc_xid_xbox_gametrak;

    memset(&s->in_state_capabilities, 0xFF, sizeof(s->in_state_capabilities));
    s->in_state_capabilities.bLength = sizeof(s->in_state_capabilities);
    s->in_state_capabilities.bReportId = 0;

    memset(&s->out_state_capabilities, 0xFF, sizeof(s->out_state_capabilities));
    s->out_state_capabilities.length = sizeof(s->out_state_capabilities);
    s->out_state_capabilities.report_id = 0;
}

static void update_gametrak_input(USBXIDGamepadState *s)
{
    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    xemu_input_update_controller(state);

    s->in_state.bLength = 0x14;
    s->in_state.wButtons = 0x0000;
    s->in_state.bAnalogButtons[2] = 0xff;
    s->in_state.bAnalogButtons[3] = 0xff;
    s->in_state.bAnalogButtons[4] = 0x0f;
    memset((char*)&s->in_state+2, 0xFF, 0x12);
}

static void xbox_gametrak_handle_data(USBDevice *dev, USBPacket *p) {
    USBXIDGamepadState *s = DO_UPCAST(USBXIDGamepadState, dev, dev);

    switch (p->pid) {
        case USB_TOKEN_IN:
            update_gametrak_input(s);
            usb_packet_copy(p, &s->in_state, s->in_state.bLength);
            //*
            fprintf(stderr, "xbox_gametrak_handle_data : len=0x%02x / data=", p->actual_length);
            for (int i = 0; i < p->actual_length && i < 32; i++) {
                fprintf(stderr, "%02x ", *((uint8_t*)&s->in_state + i));
            }
            fprintf(stderr, "\n");
            //*/
            break;
        case USB_TOKEN_OUT:
        default:
            break;
    }
}

static Property xid_properties[] = {
    DEFINE_PROP_UINT8("index", USBXIDGamepadState, device_index, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void xbox_gametrak_class_init(ObjectClass *klass, void *class_data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Xbox Gametrak";
    uc->usb_desc       = &desc_xbox_gametrak;
    uc->realize        = xbox_gametrak_realize;
    uc->handle_control = usb_xid_handle_control;
    uc->handle_data    = xbox_gametrak_handle_data;

    device_class_set_props(dc, xid_properties);
    dc->desc = "Xbox Gametrak";
}

static const TypeInfo xbox_gametrak_info = {
    .name          = TYPE_USB_XID_GAMETRAK,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDGamepadState),
    .class_init    = xbox_gametrak_class_init,
};

static void usb_xbox_gametrak_register_types(void) {
    type_register_static(&xbox_gametrak_info);
}

type_init(usb_xbox_gametrak_register_types)
