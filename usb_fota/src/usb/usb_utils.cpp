#include "usb_utils.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

#include "libusb-1.0/libusb.h"

namespace usb
{
#define append_txt(ss, text) (ss) << (text) << "\n"
#define append_hex(ss, prefix, v) (ss) << (prefix) << std::hex << static_cast<int>(v) << std::dec << "h\n"
#define append_dec(ss, prefix, v) (ss) << (prefix) << static_cast<int>(v) << "\n"

	static void enumerate_endpoint_comp_info(std::stringstream& ss, const struct libusb_ss_endpoint_companion_descriptor* ep_comp)
	{
		append_txt(ss, "      USB 3.0 Endpoint Companion:");
		append_dec(ss, "        bMaxBurst:           ", ep_comp->bMaxBurst);
		append_hex(ss, "        bmAttributes:        ", ep_comp->bmAttributes);
		append_dec(ss, "        wBytesPerInterval:   ", ep_comp->wBytesPerInterval);
	}
	static void enumerate_endpoint_info(std::stringstream& ss, endpoint_t& _endpoint, const struct libusb_endpoint_descriptor* endpoint)
	{
		int i, ret;

		append_txt(ss, "      Endpoint:");
		append_hex(ss, "        bEndpointAddress:    ", endpoint->bEndpointAddress);
		append_hex(ss, "        bmAttributes:        ", endpoint->bmAttributes);
		append_dec(ss, "        wMaxPacketSize:      ", endpoint->wMaxPacketSize);
		append_dec(ss, "        bInterval:           ", endpoint->bInterval);
		append_dec(ss, "        bRefresh:            ", endpoint->bRefresh);
		append_dec(ss, "        bSynchAddress:       ", endpoint->bSynchAddress);

		_endpoint.bEndpointAddress = endpoint->bEndpointAddress;
		_endpoint.bmAttributes = endpoint->bmAttributes;
		_endpoint.wMaxPacketSize = endpoint->wMaxPacketSize;

		for (i = 0; i < endpoint->extra_length;) {
			if (LIBUSB_DT_SS_ENDPOINT_COMPANION == endpoint->extra[i + 1]) {
				struct libusb_ss_endpoint_companion_descriptor* ep_comp;

				ret = libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
				if (LIBUSB_SUCCESS != ret)
					continue;

				enumerate_endpoint_comp_info(ss, ep_comp);

				libusb_free_ss_endpoint_companion_descriptor(ep_comp);
			}

			i += endpoint->extra[i];
		}
	}

	static void enumerate_altsetting_info(std::stringstream& ss, interface_t& _interface, const struct libusb_interface_descriptor* interface0)
	{
		uint8_t i;

		append_txt(ss, "    Interface:");
		append_dec(ss, "      bInterfaceNumber:      ", interface0->bInterfaceNumber);
		append_dec(ss, "      bAlternateSetting:     ", interface0->bAlternateSetting);
		append_dec(ss, "      bNumEndpoints:         ", interface0->bNumEndpoints);
		append_dec(ss, "      bInterfaceClass:       ", interface0->bInterfaceClass);
		append_dec(ss, "      bInterfaceSubClass:    ", interface0->bInterfaceSubClass);
		append_dec(ss, "      bInterfaceProtocol:    ", interface0->bInterfaceProtocol);
		append_dec(ss, "      iInterface:            ", interface0->iInterface);

		_interface.bInterfaceNumber = interface0->bInterfaceNumber;
		_interface.bAlternateSetting = interface0->bAlternateSetting;
		_interface.bNumEndpoints = interface0->bNumEndpoints;
		_interface.bInterfaceClass = interface0->bInterfaceClass;
		_interface.bInterfaceSubClass = interface0->bInterfaceSubClass;
		_interface.bInterfaceProtocol = interface0->bInterfaceProtocol;

		for (i = 0; i < interface0->bNumEndpoints; i++)
		{
			endpoint_t _endpoint;
			_endpoint.bInterfaceNumber = interface0->bInterfaceNumber;
			enumerate_endpoint_info(ss, _endpoint, &interface0->endpoint[i]);
			_interface.endpointList.push_back(_endpoint);
		}
	}

	static void enumerate_2_0_ext_cap_info(std::stringstream& ss, struct libusb_usb_2_0_extension_descriptor* usb_2_0_ext_cap)
	{
		printf("    USB 2.0 Extension Capabilities:\n");
		printf("      bDevCapabilityType:    %u\n", usb_2_0_ext_cap->bDevCapabilityType);
		printf("      bmAttributes:          %08xh\n", usb_2_0_ext_cap->bmAttributes);
	}
	static void enumerate_ss_usb_cap_info(std::stringstream& ss, struct libusb_ss_usb_device_capability_descriptor* ss_usb_cap)
	{
		printf("    USB 3.0 Capabilities:\n");
		printf("      bDevCapabilityType:    %u\n", ss_usb_cap->bDevCapabilityType);
		printf("      bmAttributes:          %02xh\n", ss_usb_cap->bmAttributes);
		printf("      wSpeedSupported:       %u\n", ss_usb_cap->wSpeedSupported);
		printf("      bFunctionalitySupport: %u\n", ss_usb_cap->bFunctionalitySupport);
		printf("      bU1devExitLat:         %u\n", ss_usb_cap->bU1DevExitLat);
		printf("      bU2devExitLat:         %u\n", ss_usb_cap->bU2DevExitLat);
	}
	static void enumerate_bos_info(std::stringstream& ss, libusb_device_handle* handle)
	{
		struct libusb_bos_descriptor* bos;
		uint8_t i;
		int ret;

		ret = libusb_get_bos_descriptor(handle, &bos);
		if (ret < 0)
			return;

		printf("  Binary Object Store (BOS):\n");
		printf("    wTotalLength:            %u\n", bos->wTotalLength);
		printf("    bNumDeviceCaps:          %u\n", bos->bNumDeviceCaps);

		for (i = 0; i < bos->bNumDeviceCaps; i++) {
			struct libusb_bos_dev_capability_descriptor* dev_cap = bos->dev_capability[i];

			if (dev_cap->bDevCapabilityType == LIBUSB_BT_USB_2_0_EXTENSION) {
				struct libusb_usb_2_0_extension_descriptor* usb_2_0_extension;

				ret = libusb_get_usb_2_0_extension_descriptor(NULL, dev_cap, &usb_2_0_extension);
				if (ret < 0)
					return;

				enumerate_2_0_ext_cap_info(ss, usb_2_0_extension);
				libusb_free_usb_2_0_extension_descriptor(usb_2_0_extension);
			}
			else if (dev_cap->bDevCapabilityType == LIBUSB_BT_SS_USB_DEVICE_CAPABILITY) {
				struct libusb_ss_usb_device_capability_descriptor* ss_dev_cap;

				ret = libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_dev_cap);
				if (ret < 0)
					return;

				enumerate_ss_usb_cap_info(ss, ss_dev_cap);
				libusb_free_ss_usb_device_capability_descriptor(ss_dev_cap);
			}
		}

		libusb_free_bos_descriptor(bos);
	}

	static void enumerate_interface_info(std::stringstream& ss, configuration_t& _configuration, const struct libusb_interface* interface)
	{
		for (int i = 0; i < interface->num_altsetting; i++)
		{
			interface_t _interface;
			_interface.index = i;
			enumerate_altsetting_info(ss, _interface, &interface->altsetting[i]);
			_configuration.interfaceList.push_back(_interface);
		}
	}
	static void enumerate_configuration_info(std::stringstream& ss, configuration_t& _configuration, struct libusb_config_descriptor* config)
	{
		uint8_t i;

		append_txt(ss, "  Configuration:");
		append_dec(ss, "    wTotalLength:            ", config->wTotalLength);
		append_dec(ss, "    bNumInterfaces:          ", config->bNumInterfaces);
		append_dec(ss, "    bConfigurationValue:     ", config->bConfigurationValue);
		append_dec(ss, "    iConfiguration:          ", config->iConfiguration);
		append_hex(ss, "    bmAttributes:            ", config->bmAttributes);
		append_dec(ss, "    MaxPower:                ", config->MaxPower);

		_configuration.wTotalLength = config->wTotalLength;
		_configuration.bNumInterfaces = config->bNumInterfaces;
		_configuration.bConfigurationValue = config->bConfigurationValue;
		_configuration.bmAttributes = config->bmAttributes;
		_configuration.MaxPower = config->MaxPower;

		for (i = 0; i < config->bNumInterfaces; i++)
			enumerate_interface_info(ss, _configuration, &config->interface[i]);
	}

	void enumerate_device_info(libusb_device* lib_usb_device, device& device)
	{
		struct libusb_device_descriptor desc;
		const char* speed;
		int ret;
		uint8_t i;
		char buff[512] = {0};

		switch (libusb_get_device_speed(lib_usb_device)) {
		case LIBUSB_SPEED_LOW:			speed = "1.5M"; break;
		case LIBUSB_SPEED_FULL:			speed = "12M";	break;
		case LIBUSB_SPEED_HIGH:			speed = "480M"; break;
		case LIBUSB_SPEED_SUPER:		speed = "5G";	break;
		case LIBUSB_SPEED_SUPER_PLUS:	speed = "10G";	break;
		default:						speed = "Unknown";
		}

		if (libusb_get_device_descriptor(lib_usb_device, &desc)) {
			throw std::exception("failed to get device descriptor");
		}

		// Read String descriptor 'Manufacturer' and 'Product'
		if (desc.iManufacturer != 0 || desc.iProduct != 0) {
			libusb_device_handle* handler = NULL;
			if (libusb_open(lib_usb_device, &handler) == LIBUSB_SUCCESS) {
				if (desc.iManufacturer != 0) {
					libusb_get_string_descriptor_ascii(handler, desc.iManufacturer, (unsigned char*)buff, 512);
					device.manufacturer = buff;
				}
				if (desc.iProduct != 0) {
					libusb_get_string_descriptor_ascii(handler, desc.iProduct, (unsigned char*)buff, 512);
					device.product = buff;
				}
				libusb_close(handler);
			}
		}

		device.vid = desc.idVendor;
		device.pid = desc.idProduct;
		sprintf(buff, "VID%04x&PID%04x", device.vid, device.pid);
		device.name = buff;
		
		std::stringstream ss;
		for (i = 0; i < desc.bNumConfigurations; i++) {
			struct libusb_config_descriptor* config;

			ret = libusb_get_config_descriptor(lib_usb_device, i, &config);
			if (LIBUSB_SUCCESS != ret) {
				printf("  Couldn't retrieve descriptors\n");
				continue;
			}

			configuration_t configuration_info;
			enumerate_configuration_info(ss, configuration_info, config);
			device.configurationList.push_back(configuration_info);

			libusb_free_config_descriptor(config);
		}
		device.descriptor_text = ss.str();
	}

	const std::unordered_map<uint32_t, device> enumerate_all_device() 
	{
		std::unordered_map<uint32_t, device> map;
		size_t cnt;

		libusb_context* libusb_ctx = NULL;
		libusb_device** libusb_device_list = NULL;

		// init libusb
		if (libusb_init(&libusb_ctx) < 0)
			throw std::exception("failed to init libusb.");

		// enumerate all device
		cnt = libusb_get_device_list(libusb_ctx, &libusb_device_list);

		for (int i = 0; i < cnt; ++i)
		{
			device device;
			enumerate_device_info(libusb_device_list[i], device);

			uint32_t id = device.vid << 16 | device.pid;

			map[id] = device;
		}

		// free
		libusb_free_device_list(libusb_device_list, 1);
		libusb_exit(libusb_ctx);
		return map;
	}
};
