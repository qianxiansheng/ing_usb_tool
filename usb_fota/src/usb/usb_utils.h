#pragma once

#include "pch.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace usb
{
	struct endpoint_t
	{
		uint8_t bEndpointAddress;
		uint8_t bmAttributes;
		uint16_t wMaxPacketSize;

		uint8_t bInterfaceNumber;	//Parent interface index
	};

	struct interface_t
	{
		uint8_t index;
		uint8_t bInterfaceNumber;
		uint8_t bAlternateSetting;
		uint8_t bNumEndpoints;
		uint8_t bInterfaceClass;
		uint8_t bInterfaceSubClass;
		uint8_t bInterfaceProtocol;

		std::vector<endpoint_t> endpointList;
	};

	struct configuration_t
	{
		uint16_t wTotalLength;
		uint8_t bNumInterfaces;
		uint8_t bConfigurationValue;
		uint8_t bmAttributes;
		uint8_t MaxPower;

		std::vector<interface_t> interfaceList;
	};

	struct device
	{
		std::string name;
		std::string manufacturer;
		std::string product;
		uint16_t vid;
		uint16_t pid;
		std::string descriptor_text;

		std::vector<configuration_t> configurationList;
	};

	const std::unordered_map<uint32_t, device> enumerate_all_device();
};
