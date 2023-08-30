/*
* Test suite program based of libusb-0.1-compat testlibusb
* Copyright (c) 2013 Nathan Hjelm <hjelmn@mac.ccom>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <iostream>
#include <string.h>
#include "libusb-1.0/libusb.h"

int verbose = 1;
int VID = 0x174F;
int PID = 0x244C;

static void print_endpoint_comp(const struct libusb_ss_endpoint_companion_descriptor* ep_comp)
{
	printf("      USB 3.0 Endpoint Companion:\n");
	printf("        bMaxBurst:           %u\n", ep_comp->bMaxBurst);
	printf("        bmAttributes:        %02xh\n", ep_comp->bmAttributes);
	printf("        wBytesPerInterval:   %u\n", ep_comp->wBytesPerInterval);
}

static void print_endpoint(const struct libusb_endpoint_descriptor* endpoint)
{
	int i, ret;

	printf("      Endpoint:\n");
	printf("        bEndpointAddress:    %02xh\n", endpoint->bEndpointAddress);
	printf("        bmAttributes:        %02xh\n", endpoint->bmAttributes);
	printf("        wMaxPacketSize:      %u\n", endpoint->wMaxPacketSize);
	printf("        bInterval:           %u\n", endpoint->bInterval);
	printf("        bRefresh:            %u\n", endpoint->bRefresh);
	printf("        bSynchAddress:       %u\n", endpoint->bSynchAddress);

	for (i = 0; i < endpoint->extra_length;) {
		if (LIBUSB_DT_SS_ENDPOINT_COMPANION == endpoint->extra[i + 1]) {
			struct libusb_ss_endpoint_companion_descriptor* ep_comp;

			ret = libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
			if (LIBUSB_SUCCESS != ret)
				continue;

			print_endpoint_comp(ep_comp);

			libusb_free_ss_endpoint_companion_descriptor(ep_comp);
		}

		i += endpoint->extra[i];
	}
}

static void print_altsetting(const struct libusb_interface_descriptor* interface)
{
	uint8_t i;

	printf("    Interface:\n");
	printf("      bInterfaceNumber:      %u\n", interface->bInterfaceNumber);
	printf("      bAlternateSetting:     %u\n", interface->bAlternateSetting);
	printf("      bNumEndpoints:         %u\n", interface->bNumEndpoints);
	printf("      bInterfaceClass:       %u\n", interface->bInterfaceClass);
	printf("      bInterfaceSubClass:    %u\n", interface->bInterfaceSubClass);
	printf("      bInterfaceProtocol:    %u\n", interface->bInterfaceProtocol);
	printf("      iInterface:            %u\n", interface->iInterface);

	for (i = 0; i < interface->bNumEndpoints; i++)
		print_endpoint(&interface->endpoint[i]);
}

static void print_2_0_ext_cap(struct libusb_usb_2_0_extension_descriptor* usb_2_0_ext_cap)
{
	printf("    USB 2.0 Extension Capabilities:\n");
	printf("      bDevCapabilityType:    %u\n", usb_2_0_ext_cap->bDevCapabilityType);
	printf("      bmAttributes:          %08xh\n", usb_2_0_ext_cap->bmAttributes);
}

static void print_ss_usb_cap(struct libusb_ss_usb_device_capability_descriptor* ss_usb_cap)
{
	printf("    USB 3.0 Capabilities:\n");
	printf("      bDevCapabilityType:    %u\n", ss_usb_cap->bDevCapabilityType);
	printf("      bmAttributes:          %02xh\n", ss_usb_cap->bmAttributes);
	printf("      wSpeedSupported:       %u\n", ss_usb_cap->wSpeedSupported);
	printf("      bFunctionalitySupport: %u\n", ss_usb_cap->bFunctionalitySupport);
	printf("      bU1devExitLat:         %u\n", ss_usb_cap->bU1DevExitLat);
	printf("      bU2devExitLat:         %u\n", ss_usb_cap->bU2DevExitLat);
}

static void print_bos(libusb_device_handle* handle)
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

			print_2_0_ext_cap(usb_2_0_extension);
			libusb_free_usb_2_0_extension_descriptor(usb_2_0_extension);
		}
		else if (dev_cap->bDevCapabilityType == LIBUSB_BT_SS_USB_DEVICE_CAPABILITY) {
			struct libusb_ss_usb_device_capability_descriptor* ss_dev_cap;

			ret = libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_dev_cap);
			if (ret < 0)
				return;

			print_ss_usb_cap(ss_dev_cap);
			libusb_free_ss_usb_device_capability_descriptor(ss_dev_cap);
		}
	}

	libusb_free_bos_descriptor(bos);
}

static void print_interface(const struct libusb_interface* interface)
{
	int i;

	for (i = 0; i < interface->num_altsetting; i++)
		print_altsetting(&interface->altsetting[i]);
}

static void print_configuration(struct libusb_config_descriptor* config)
{
	uint8_t i;

	printf("  Configuration:\n");
	printf("    wTotalLength:            %u\n", config->wTotalLength);
	printf("    bNumInterfaces:          %u\n", config->bNumInterfaces);
	printf("    bConfigurationValue:     %u\n", config->bConfigurationValue);
	printf("    iConfiguration:          %u\n", config->iConfiguration);
	printf("    bmAttributes:            %02xh\n", config->bmAttributes);
	printf("    MaxPower:                %u\n", config->MaxPower);

	for (i = 0; i < config->bNumInterfaces; i++)
		print_interface(&config->interface[i]);
}

static void print_device(libusb_device* dev, libusb_device_handle* handle)
{
	struct libusb_device_descriptor desc;
	unsigned char string[256];
	const char* speed;
	int ret;
	uint8_t i;

	switch (libusb_get_device_speed(dev)) {
	case LIBUSB_SPEED_LOW:		speed = "1.5M"; break;
	case LIBUSB_SPEED_FULL:		speed = "12M"; break;
	case LIBUSB_SPEED_HIGH:		speed = "480M"; break;
	case LIBUSB_SPEED_SUPER:	speed = "5G"; break;
	case LIBUSB_SPEED_SUPER_PLUS:	speed = "10G"; break;
	default:			speed = "Unknown";
	}

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return;
	}

	printf("Dev (bus %u, device %u): %04X - %04X speed: %s\n",
		libusb_get_bus_number(dev), libusb_get_device_address(dev),
		desc.idVendor, desc.idProduct, speed);

	if (!handle)
		libusb_open(dev, &handle);

	if (handle) {
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				printf("  Manufacturer:              %s\n", (char*)string);
		}

		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				printf("  Product:                   %s\n", (char*)string);
		}

		if (desc.iSerialNumber && verbose) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
			if (ret > 0)
				printf("  Serial Number:             %s\n", (char*)string);
		}
	}

	if (verbose && (VID == desc.idVendor) && (PID == desc.idProduct)) {
		for (i = 0; i < desc.bNumConfigurations; i++) {
			struct libusb_config_descriptor* config;

			ret = libusb_get_config_descriptor(dev, i, &config);
			if (LIBUSB_SUCCESS != ret) {
				printf("  Couldn't retrieve descriptors\n");
				continue;
			}

			print_configuration(config);

			libusb_free_config_descriptor(config);
		}

		if (handle && desc.bcdUSB >= 0x0201)
			print_bos(handle);
	}

	if (handle)
		libusb_close(handle);
}

#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int test_wrapped_device(const char* device_name)
{
	libusb_device_handle* handle;
	int r, fd;

	fd = open(device_name, O_RDWR);
	if (fd < 0) {
		printf("Error could not open %s: %s\n", device_name, strerror(errno));
		return 1;
	}
	r = libusb_wrap_sys_device(NULL, fd, &handle);
	if (r) {
		printf("Error wrapping device: %s: %s\n", device_name, libusb_strerror(r));
		close(fd);
		return 1;
	}
	print_device(libusb_get_device(handle), handle);
	close(fd);
	return 0;
}
#else
static int test_wrapped_device(const char* device_name)
{
	(void)device_name;
	printf("Testing wrapped devices is not supported on your platform\n");
	return 1;
}
#endif

static void printf_hexdump(uint8_t* data, uint16_t size)
{
	std::cout << std::hex;
	for (uint16_t i = 0; i < size; ++i) {
		std::cout << (int)data[i];
	}
	std::cout << std::dec << std::endl;
}

int main1()
{
	// 初使化上下文
	int nRet = libusb_init(NULL);
	if (nRet < 0)
	{
		printf("libusb_init(NULL) failed:[%s] \n", libusb_strerror(nRet));
		return -1;
	}

	printf("libusb_init(NULL) ok \n");

	// 打开指定厂商的某类产品
	libusb_device_handle* pHandle = libusb_open_device_with_vid_pid(NULL, 0xFFF1, 0xFA2F);
	if (pHandle == NULL)
	{
		printf("libusb_open_device_with_vid_pid(0xFFF1, 0xFA2F) failed \n");
		libusb_exit(NULL);
		return -1;
	}

	printf("libusb_open_device_with_vid_pid(0xFFF1, 0xFA2F) ok \n");

	// 声明使用第1个接口
	nRet = libusb_claim_interface(pHandle, 0);
	if (nRet < 0)
	{
		printf("libusb_claim_interface(0) failed:[%s] \n", libusb_strerror(nRet));
		libusb_close(pHandle);
		libusb_exit(NULL);
		return -1;
	}

	printf("libusb_claim_interface(0) ok \n");

	// 向指定端点发送数据

	char sBuf[64] = "1234567890";
	int nActualBytes = 0;

	std::cout << "Send data to EP OUT:" << std::endl;
	printf_hexdump((uint8_t*)sBuf, 64);
	
	nRet = libusb_bulk_transfer(pHandle, 0x02, (unsigned char*)sBuf, 64, &nActualBytes, 1000);
	if (nRet < 0)
	{
		printf("libusb_bulk_transfer(0x02) write failed:[%s] \n", libusb_strerror(nRet));

		libusb_release_interface(pHandle, 0);
		libusb_close(pHandle);
		libusb_exit(NULL);
		return -1;
	}

	printf("libusb_bulk_transfer(0x02) write size:[%d] \n", nActualBytes);

	// 从指定端点接收数据
	char sBuf2[10] = { 0 };
	nActualBytes = 0;
	nRet = libusb_bulk_transfer(pHandle, 0x81, (unsigned char*)sBuf2, sizeof(sBuf2), &nActualBytes, 1000);
	if (nRet < 0)
	{
		printf("libusb_bulk_transfer(0x81) read failed:[%s] \n", libusb_strerror(nRet));
		libusb_release_interface(pHandle, 0);
		libusb_close(pHandle);
		libusb_exit(NULL);
		return -1;
	}

	printf("libusb_bulk_transfer(0x81) read size:[%d] \n", nActualBytes);

	std::cout << "Recv response:" << std::endl;
	printf_hexdump((uint8_t*)sBuf2, nActualBytes);

	// 释放第1个接口
	libusb_release_interface(pHandle, 0);

	// 关闭设备
	libusb_close(pHandle);

	// 释放上下文
	libusb_exit(NULL);

	return 0;
}

int main2(int argc, char* argv[])
{
	const char* device_name = NULL;
	libusb_device** devs;
	ssize_t cnt;
	int r, i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			verbose = 1;
		}
		else if (!strcmp(argv[i], "-d") && (i + 1) < argc) {
			i++;
			device_name = argv[i];
		}
		else {
			printf("Usage %s [-v] [-d </dev/bus/usb/...>]\n", argv[0]);
			printf("Note use -d to test libusb_wrap_sys_device()\n");
			return 1;
		}
	}

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	if (device_name) {
		r = test_wrapped_device(device_name);
	}
	else {
		cnt = libusb_get_device_list(NULL, &devs);
		if (cnt < 0) {
			libusb_exit(NULL);
			return 1;
		}

		for (i = 0; devs[i]; i++)
			print_device(devs[i], NULL);

		libusb_free_device_list(devs, 1);
	}

	libusb_exit(NULL);
	return r;
}


#include "crcLib.h"
uint16_t crc16_modbus2(uint8_t* data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;        // 0xA001 = reverse 0x8005
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}


#include <algorithm>
#include <fstream>

int main5(int argc, char* argv[])
{
	uint8_t data[] = { 
		0x11, 0x22, 0x33, 0x44, 0x44, 0x44, 0x44, 0x45, 
		0x44, 0x44, 0x45, 0x63, 0x46, 0x34, 0x56, 0x34, 
		0x3D, 0xDD, 0xDE, 0xDD, 0xEC, 0xF4, 0x00, 0x22, 
		0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 
	};

	uint16_t crc = crc16_modbus2(data, 32);

	std::cout << std::hex << (int)crc << std::dec << std::endl;

	std::ofstream ofs;
	ofs.open("crctest.bin", std::ios::binary | std::ios::out);
	ofs.write((char*)data, 32);
	ofs.close();


	return 0;
}

#include "aes.h"

#define AES_BLOCK_LEN 16

void aes_cbc_pcsk7_encrypt(const uint8_t* key, const uint8_t* iv, uint8_t* data, size_t size, uint8_t* outData, size_t* outSize)
{
	size_t l = AES_BLOCK_LEN - size;
	if (l == 0) l = AES_BLOCK_LEN;
	size_t s = size + l;
	uint8_t* buf = (uint8_t*)malloc(s + 100);
	if (buf == NULL)
		return;
	memcpy(buf, data, size);
	memset(buf + size, l, l);

	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, iv);
	AES_CBC_encrypt_buffer(&ctx, buf, s);

	memcpy(outData, buf, s);
	*outSize = s;

	free(buf);
}
void aes_cbc_pcsk7_decrypt(const uint8_t* key, const uint8_t* iv, uint8_t* data, size_t size, uint8_t* outData, size_t* outSize)
{
	uint8_t* buf = (uint8_t*)malloc(size + 100);
	if (buf == NULL)
		return;
	memcpy(buf, data, size);

	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, iv);
	AES_CBC_decrypt_buffer(&ctx, buf, size);

	int n = buf[size - 1];
	int s = size - n;
	memcpy(outData, buf, s);
	*outSize = s;

	free(buf);
}

int main4()
{
	const uint8_t key[17] = "HefSB4whqSG5Ly9L";
	const uint8_t iv[17] = "apNgbnsyy6bAXnNm";
	uint8_t data[16] = "Hello World!";

	uint8_t encrypt_buf[16] = { 0 };
	size_t encrypt_size = 0;

	uint8_t decrypt_buf[16] = { 0 };
	size_t decrypt_size = 0;

	aes_cbc_pcsk7_encrypt(key, iv, data, 12, encrypt_buf, &encrypt_size);
	
	for (uint8_t i = 0; i < encrypt_size; ++i)
	{
		std::cout << std::hex << (int)encrypt_buf[i] << " ";
	}
	std::cout << std::endl;

	aes_cbc_pcsk7_decrypt(key, iv, encrypt_buf, encrypt_size, decrypt_buf, &decrypt_size);
	
	std::cout << "Over" << std::endl;

	return 0;
}


#include <stdio.h>
#include <libusb-1.0/libusb.h>

static void print_devs(libusb_device** devs)
{
	libusb_device* dev;
	int i = 0;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return;
		}

		printf("%04x:%04x (bus %d, device %d)\n",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));
	}
}

void send_cb(struct libusb_transfer* transfer)
{
	printf("Send Completed!\n");
}

int main8()
{
	int r;
	ssize_t cnt;
	libusb_device_handle* dev_handle;             //a device handle
	libusb_device** devs;                         //devices
	//libusb_context **ctx=NULL;	
	r = libusb_init(NULL);                          //init 初始化libusb
	if (r < 0) {
		printf("failed to init libusb\n");
		return 1;
	}
	cnt = libusb_get_device_list(NULL, &devs);      //获取设备列表
	if (cnt < 0) {
		printf("failed to get device list\n");
		return 1;
	}
	//print_devs(devs);
	dev_handle = libusb_open_device_with_vid_pid(NULL, 0xFFFF, 0xFA2F);
	if (dev_handle == NULL) {
		printf("Cannot open device\n");
		return 1;
	}
	else
		printf("Device Opened\n");

	libusb_free_device_list(devs, 1);             //free the list, unref the devices in it

	if (libusb_kernel_driver_active(dev_handle, 0) == 1) { //find out if kernel driver is attached
		printf("Kernel Driver Active\n");
		if (libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
			printf("Kernel Driver Detached!\n");
	}
	r = libusb_claim_interface(dev_handle, 0);         //claim interface 0 (the first) of device (mine had jsut 1)
	if (r < 0) {
		printf("Cannot Claim Interface\n");
		return 1;
	}
	printf("Claimed Interface\n");


	uint8_t sendBuf[64] = {0x12, 0x34, 0x56, 0x78};
	libusb_transfer* transfer = libusb_alloc_transfer(0);

	libusb_fill_interrupt_transfer(transfer, dev_handle, 2, sendBuf, 64, send_cb, NULL, 1000);
	r = libusb_submit_transfer(transfer);
	if (r == 0)
	{
		printf("send submit ok\n");
		libusb_free_transfer(transfer);
	}
	else
	{
		printf("error %s\n", libusb_strerror(r));

		libusb_free_transfer(transfer);
	}


	r = libusb_release_interface(dev_handle, 0); //release the claimed interface
	if (r != 0) {
		printf("Cannot Release Interface\n");
		return 1;
	}
	printf("Released Interface\n");

	libusb_close(dev_handle);                    //close the device we opened
	libusb_exit(NULL);                            //needs to be called to end the

}

int main7()
{
	int r;
	ssize_t cnt;
	libusb_device_handle* dev_handle;             //a device handle
	libusb_device** devs;                         //devices
	//libusb_context **ctx=NULL;	
	r = libusb_init(NULL);                          //init 初始化libusb
	if (r < 0) {
		printf("failed to init libusb\n");
		return 1;
	}
	cnt = libusb_get_device_list(NULL, &devs);      //获取设备列表
	if (cnt < 0) {
		printf("failed to get device list\n");
		return 1;
	}
	//print_devs(devs);
	dev_handle = libusb_open_device_with_vid_pid(NULL, 0xFFFF, 0xFA2F);
	if (dev_handle == NULL) {
		printf("Cannot open device\n");
		return 1;
	}
	else
		printf("Device Opened\n");

	libusb_free_device_list(devs, 1);             //free the list, unref the devices in it

	if (libusb_kernel_driver_active(dev_handle, 0) == 1) { //find out if kernel driver is attached
		printf("Kernel Driver Active\n");
		if (libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
			printf("Kernel Driver Detached!\n");
	}
	r = libusb_claim_interface(dev_handle, 0);         //claim interface 0 (the first) of device (mine had jsut 1)
	if (r < 0) {
		printf("Cannot Claim Interface\n");
		return 1;
	}
	printf("Claimed Interface\n");
	Sleep(1);
	unsigned char data[2];
	union f_to_char {
		char chr[4];
		float ft0;
	}temp_union;
	unsigned char tmp_char[64];
	data[0] = 0x02; data[1] = 0x64;
	int actual; //used to find out how many bytes were written
	while (1) {
		r = libusb_interrupt_transfer(dev_handle, (0x02 | LIBUSB_ENDPOINT_OUT), data, 2, &actual, 0); //my device's out endpoint was 1, found with trial- the device had 2 endpoints: 2 and 129
		if (r == 0 && actual == 2) //we wrote the 2 bytes successfully
			printf("Writing Successful\n");
		else
			printf("Write Error %s\n", libusb_strerror(r));

		r = libusb_interrupt_transfer(dev_handle, (0x01 | LIBUSB_ENDPOINT_IN), tmp_char, 8, &actual, 1000);//pay attion

		if (r == 0 && actual == 9) //we read the 64 bytes successfully
			printf("Read Successful\n");
		else
			printf("Read Error\n");
		//printf("%i,%i\n",r,actual);
		temp_union.chr[0] = tmp_char[5];
		temp_union.chr[1] = tmp_char[6];
		temp_union.chr[2] = tmp_char[7];
		temp_union.chr[3] = tmp_char[8];
		printf("The volt is %f mV\n", temp_union.ft0);
		Sleep(500);
		printf("%s", "\033[1H\033[2J");//clear display

	}

	r = libusb_release_interface(dev_handle, 0); //release the claimed interface
	if (r != 0) {
		printf("Cannot Release Interface\n");
		return 1;
	}
	printf("Released Interface\n");

	libusb_close(dev_handle);                    //close the device we opened
	libusb_exit(NULL);                            //needs to be called to end the

	return 0;
}

union {
	uint16_t s;
	uint8_t c[sizeof(uint16_t)];
}un;

int main()
{
	uint8_t data[] = { 0x11, 0x22, 0x33, 0x44, 0x44, 0x44, 0x44, 0x45, 0x44, 0x44, 0x45, 0x63, 0x46, 0x34, 0x56, 0x34, 0x3D, 0xDD, 0xDE, 0xDD, 0xEC, 0xF4, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 };

	uint16_t r = crc16_modbus(data, sizeof(data));

	uint8_t lsb = r;
	uint8_t msb = r >> 8;

	printf("%02X %02X\n", lsb, msb);

	uint16_t c = 0x8572;
	uint8_t* pc = (uint8_t*)&c;
	printf("%02X %02X\n", pc[0], pc[1]);

	un.s = 0x8572;
	printf("%02X %02X\n", un.c[0], un.c[1]);
}