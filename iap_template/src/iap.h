#pragma once

#include <iostream>
#include <assert.h>
#include "pch.h"
#include <util/utils.h>
#include "hidapi.h"
#include <vector>
#include "usb.h"

#define log 

enum IAPStatus
{
	IAP_STATUS_READY = 0x00,
	IAP_STATUS_RUNNING,
	IAP_STATUS_TERMINATE,
	IAP_STATUS_COMPLETE,

	IAP_STATUS_SEND_START_CMD = 0x20,
	IAP_STATUS_SEND_WRITE_FLASH,
	IAP_STATUS_SEND_SWITCH_APP,
	IAP_STATUS_SEND_SWITCH_BOOT,
	IAP_STATUS_END,

	IAP_STATUS_SUBSTATUS_WRITE_DATA = 0x40,
	IAP_STATUS_SUBSTATUS_READ_ACK,
};

enum IAPTerminateReason
{
	IAP_TERMINATE_REASON_OVER_THE_MAX_RETRY_COUNT = 0x00,
	IAP_TERMINATE_REASON_PROTOCOL_ERROR,
	IAP_TERMINATE_REASON_DEVICE_LOST,
};

enum iap_cmd_e
{
	IAP_CMD_START = 0xA0,
	IAP_CMD_FLASH_WRITE = 0xA1,
	IAP_CMD_FLASH_READ = 0xA2,
	IAP_CMD_REBOOT = 0xA3,
	IAP_CMD_SWITCH_APP = 0xA4,

	IAP_CMD_SWITCH_BOOT = 0xC0,

	IAP_CMD_ACK = 0xB0,
};

enum iap_transfer_ctrl_flag_bit
{
	IAP_TRANSFER_DIRECT_UP = 0x80,
	IAP_TRANSFER_DIRECT_DOWN = 0x00,
	IAP_TRANSFER_PACK_TYPE_DATA = 0x00,
	IAP_TRANSFER_PACK_TYPE_ACK = 0x40,
};

enum iap_business_ack_code_e
{
	ACK_CODE_SUCCESS = 0x00,
	ACK_CODE_UNKNOWN_CMD = 0xE0,
	ACK_CODE_LENGTH_ERORR = 0xE1,
	ACK_CODE_CRC_ERROR = 0xE2,
	ACK_CODE_BLOCK_NUM_ERROR = 0xE3,
	ACK_CODE_BLOCK_SIZE_ERROR = 0xE4,
	ACK_CODE_WRITE_OFFSET_ERROR = 0xE5,
	ACK_CODE_READ_OFFSET_ERROR = 0xE6,
	ACK_CODE_ARGUMENT_ERROR = 0xE7,
	ACK_CODE_FLASH_OPERATION_FAILED = 0xE8,
	ACK_CODE_STATUS_ERROR = 0xE9,

	ACK_CODE_HEADER_IDENTIFY_ERROR = 0xF0,
	ACK_CODE_HEADER_CHIP_ID_ERROR = 0xF1,
	ACK_CODE_HEADER_ITEM_INFO_ERROR = 0xF2,
	ACK_CODE_HEADER_HW_VERSION_ERROR = 0xF3,
	ACK_CODE_HEADER_SW_VERSION_ERROR = 0xF4,
	ACK_CODE_HEADER_CHECK_INFO_ERROR = 0xF5,
	ACK_CODE_HEADER_BLOCK_INFO_ERROR = 0xF6,
	ACK_CODE_HEADER_UPGRADE_TYPE_ERROR = 0xF7,
	ACK_CODE_HEADER_ENCRYPT_ERROR = 0xF8,
};
std::string iap_ack_str(iap_business_ack_code_e c);

class IAPContext;

typedef void (*onBusinessOkCallbackFunc)(IAPContext& ctx);


class IAPContext
{
public:
	IAPStatus primary_status;
	IAPStatus process_status;
	IAPStatus in_out_status;

	uint16_t wBlockSize;
	uint32_t wBlockNum;
	uint16_t wBlockLast;
	uint16_t wBlockIdx;

	uint32_t current_size;
	uint32_t bin_size_limit;
	uint32_t bin_size_pos;

	uint16_t delay;

	HIDDevice dev;

	int maximumRetry;
	int currentRetry;
	int readAckTimeout;
	std::vector<uint8_t> iap_bin;

	iap_business_ack_code_e ackCode;
	IAPTerminateReason terinmateReason;

	onBusinessOkCallbackFunc onBusinessSendOk;
};

class transfer_pack_exception : public std::exception {};
class business_pack_exception : public std::exception {};
class timeout_exception : public std::exception {};

void InitIAPContext(HIDDevice& dev, std::vector<uint8_t> iap_bin, uint16_t blockSize, int maximumRetry, int readAckTimeout, uint16_t delay, onBusinessOkCallbackFunc callback, IAPContext* ctx);

void iap_run(IAPContext& ctx);

void iap_run_switch_boot(IAPContext& ctx);

