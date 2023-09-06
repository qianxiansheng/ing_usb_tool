#include "iap.h"

static uint8_t hid_report_buf[IAP_USB_HID_REPORT_SIZE] = { IAP_USB_HID_REPORT_ID };
static uint8_t businessFlashWriteBuf[IAP_PRO_BUSINESS_FLASH_WRITE_BUF_MAX_SIZE] = { 0 };
static uint8_t businessAckBuf[IAP_PRO_BUSINESS_ACK_MAX_SIZE] = { 0 };

IAPContext iap_ctx;

bool stop = false;

void InitIAPContext(HIDDevice& dev, std::vector<uint8_t> iap_bin, uint16_t blockSize, int maximumRetry, int readAckTimeout, uint16_t delay, onBusinessOkCallbackFunc callback, IAPContext* ctx)
{
	ctx->dev = dev;
	ctx->iap_bin = iap_bin;

	ctx->onBusinessSendOk = callback;

	ctx->delay = delay;

	ctx->readAckTimeout = readAckTimeout;
	ctx->maximumRetry = maximumRetry;
	ctx->currentRetry = 0;
	ctx->primary_status = IAP_STATUS_READY;
	ctx->process_status = IAP_STATUS_SEND_START_CMD;
	ctx->in_out_status = IAP_STATUS_SUBSTATUS_WRITE_DATA;

	uint32_t binSize = (uint32_t)ctx->iap_bin.size() - 128;
	ctx->wBlockSize = blockSize;
	ctx->wBlockNum  = (binSize - 1) / blockSize + 1;
	ctx->wBlockLast = (binSize - 1) % blockSize + 1;
	ctx->wBlockIdx = 0;

	ctx->bin_size_limit = binSize;
	ctx->bin_size_pos = 0;
}

int hid_set_report(hid_device* dev, uint8_t* data, uint32_t size)
{
	assert(size <= IAP_USB_HID_REPORT_CONTENT_SIZE);
	if (hid_report_buf + 1 != data)
		memcpy(hid_report_buf + 1, data, size);

	return hid_write(dev, hid_report_buf, IAP_USB_HID_REPORT_SIZE);
}
int hid_get_report(hid_device* dev, uint8_t* data, uint32_t size, int timeout)
{
	assert(size <= IAP_USB_HID_REPORT_CONTENT_SIZE);

	int r = hid_read_timeout(dev, hid_report_buf, IAP_USB_HID_REPORT_SIZE, timeout);
	if (data != hid_report_buf + 1)
		memcpy(data, hid_report_buf + 1, size);

	return r;
}

void iap_transfer(HIDDevice& dev, uint8_t* data, uint32_t size, uint16_t packIdxReverse, bool isFirst)
{
	assert(size <= IAP_PRO_TRANSFER_PAYLOAD_SIZE);
	uint8_t* buf = hid_report_buf + 1;
	buf[0] = IAP_PRO_TRANSFER_HEADER;
	buf[1] = IAP_TRANSFER_DIRECT_DOWN | IAP_TRANSFER_PACK_TYPE_DATA;
	uint16_t pack_ctrl = (isFirst << 15) | (packIdxReverse & 0x7FFF);
	buf[2] = pack_ctrl & 0xFF;
	buf[3] = pack_ctrl >> 8;
	buf[4] = size & 0xFF;
	buf[5] = size >> 8;
	if (buf + 6 != data)
		memcpy(buf + 6, data, size);
	buf[6 + size] = utils::bcc(buf + 1, size + 5);


	char sbuf[64] = { 0 };
	int nRet = hid_set_report(dev.phandle, buf, size);
	if (nRet < 0)
	{
		sprintf(sbuf, "hid_set_report failed");
		throw std::exception(sbuf);
	}
}

void iap_business(HIDDevice& dev, uint8_t* data, uint32_t size)
{
	assert(size >= 0);
	uint16_t packSize = IAP_PRO_TRANSFER_PAYLOAD_SIZE;
	uint32_t packNum = (size - 1) / packSize + 1;
	uint16_t packLastSize = (size - 1) % packSize + 1;

	uint16_t currentPackSize = 0;
	uint32_t packIdx = 0;
	uint32_t packIdxReverse = 0;

	for (packIdx = 0; packIdx < packNum; ++packIdx)
	{
		currentPackSize = (packIdx == packNum - 1) ? packLastSize : packSize;
		packIdxReverse = packNum - packIdx - 1;

		iap_transfer(dev, data + (packIdx * packSize), currentPackSize, packIdxReverse, (packIdx == 0));
	}

	char hexbuf[33] = { 0 };
	utils::Hex2String(data, (unsigned char*)hexbuf, 16);
	log("[Pack send]: %s ... (size:%d)\n", hexbuf, size);
}

uint16_t iap_transfer_read(HIDDevice& dev, uint8_t* data, bool* isFirst, int timeout)
{
	uint8_t* buf = hid_report_buf + 1;
	uint16_t len = 0;
	char sbuf[64] = { 0 };
	int nRet = hid_get_report(dev.phandle, buf, IAP_USB_HID_REPORT_CONTENT_SIZE, timeout);
	if (nRet < 0)
	{
		sprintf(sbuf, "hid_get_report failed");
		throw std::exception(sbuf);
	}
	else if (nRet == 0)
	{
		throw ::timeout_exception();
	}

	if (buf[0] != IAP_PRO_TRANSFER_HEADER)
		throw transfer_pack_exception();

	len = buf[4] | (buf[5] << 8);
	//if (len != nRet - IAP_PRO_TRANSFER_ATT_SIZE)
	//	throw transfer_pack_exception();

	if (buf[len + 6] != utils::bcc(buf + 1, len + 5))
		throw transfer_pack_exception();

	*isFirst = buf[3] & 0x80;

	if (data != buf)
		memcpy(data, buf + 6, len);

	return len;
}

uint16_t iap_business_read(HIDDevice& dev, uint8_t* data, size_t size, int timeout)
{
	bool isFirst = false;
	bool isLastPack = false;
	uint16_t transferPackLen = 0;
	uint16_t businessPackLen = 0;
	uint16_t i = 0;
	do
	{
		transferPackLen = iap_transfer_read(dev, data + i, &isFirst, timeout);
		if (isFirst)
		{
			if (transferPackLen < IAP_PRO_BUSINESS_ACK_ATT_SIZE)
				throw business_pack_exception();

			businessPackLen = data[3] | (data[4] << 8);

			if (businessPackLen > IAP_PRO_BUSINESS_ACK_PAYLOAD_MAX_SIZE)
				throw business_pack_exception();
		}

		i += transferPackLen;

		if (i == businessPackLen + IAP_PRO_BUSINESS_ACK_ATT_SIZE)
			isLastPack = true;

		utils::my_sleep(1);
	} while (!isLastPack);


	if (data[0] != IAP_CMD_ACK)
		throw business_pack_exception();

	uint16_t crc_calc = utils::crc16_modbus(data, 5 + businessPackLen);
	uint16_t crc = data[businessPackLen + 5] | (data[businessPackLen + 6] << 8);
	if (crc != crc_calc)
		throw business_pack_exception();

	return businessPackLen + IAP_PRO_BUSINESS_ACK_ATT_SIZE;
}

std::string iap_ack_str(iap_business_ack_code_e c)
{
	switch (c)
	{
	case ACK_CODE_SUCCESS:
		return "ACK_CODE_SUCCESS";
	case ACK_CODE_UNKNOWN_CMD:
		return "ACK_CODE_UNKNOWN_CMD";
	case ACK_CODE_LENGTH_ERORR:
		return "ACK_CODE_LENGTH_ERORR";
	case ACK_CODE_CRC_ERROR:
		return "ACK_CODE_CRC_ERROR";
	case ACK_CODE_BLOCK_NUM_ERROR:
		return "ACK_CODE_BLOCK_NUM_ERROR";
	case ACK_CODE_BLOCK_SIZE_ERROR:
		return "ACK_CODE_BLOCK_SIZE_ERROR";
	case ACK_CODE_WRITE_OFFSET_ERROR:
		return "ACK_CODE_WRITE_OFFSET_ERROR";
	case ACK_CODE_READ_OFFSET_ERROR:
		return "ACK_CODE_READ_OFFSET_ERROR";
	case ACK_CODE_ARGUMENT_ERROR:
		return "ACK_CODE_ARGUMENT_ERROR";
	case ACK_CODE_FLASH_OPERATION_FAILED:
		return "ACK_CODE_FLASH_OPERATION_FAILED";
	case ACK_CODE_STATUS_ERROR:
		return "ACK_CODE_STATUS_ERROR";
	case ACK_CODE_HEADER_IDENTIFY_ERROR:
		return "ACK_CODE_HEADER_IDENTIFY_ERROR";
	case ACK_CODE_HEADER_CHIP_ID_ERROR:
		return "ACK_CODE_HEADER_CHIP_ID_ERROR";
	case ACK_CODE_HEADER_ITEM_INFO_ERROR:
		return "ACK_CODE_HEADER_ITEM_INFO_ERROR";
	case ACK_CODE_HEADER_HW_VERSION_ERROR:
		return "ACK_CODE_HEADER_HW_VERSION_ERROR";
	case ACK_CODE_HEADER_SW_VERSION_ERROR:
		return "ACK_CODE_HEADER_SW_VERSION_ERROR";
	case ACK_CODE_HEADER_CHECK_INFO_ERROR:
		return "ACK_CODE_HEADER_CHECK_INFO_ERROR";
	case ACK_CODE_HEADER_UPGRADE_TYPE_ERROR:
		return "ACK_CODE_HEADER_UPGRADE_TYPE_ERROR";
	case ACK_CODE_HEADER_ENCRYPT_ERROR:
		return "ACK_CODE_HEADER_ENCRYPT_ERROR";
	default:
		return "ACK_CODE_UNKNOWN_ERROR";
	}
}

void iap_handle_send_start_cmd(IAPContext& ctx)
{
	uint8_t buf[1 + 2 + IAP_HEADER_SIZE + 2] = { 0 };
	uint16_t i = 0;
	buf[i++] = IAP_CMD_START;									// CMD
	buf[i++] = IAP_HEADER_SIZE;									// LENGTH
	buf[i++] = IAP_HEADER_SIZE >> 8;
	memcpy(buf + i, ctx.iap_bin.data(), IAP_HEADER_SIZE);		// DATA
	i += IAP_HEADER_SIZE;
	uint16_t crc = utils::crc16_modbus(buf, i);
	buf[i++] = (uint8_t)(crc);										// CRC
	buf[i++] = crc >> 8;

	iap_business(ctx.dev, buf, i);
}

void iap_handle_send_reboot_cmd(IAPContext& ctx)
{
	uint8_t buf[1 + 2 + 2 + 2] = { 0 };
	uint16_t i = 0;
	buf[i++] = IAP_CMD_REBOOT;									// CMD
	buf[i++] = 2;												// LENGTH
	buf[i++] = 0;
	buf[i++] = ctx.delay & 0xFF;
	buf[i++] = ctx.delay >> 8;
	uint16_t crc = utils::crc16_modbus(buf, i);
	buf[i++] = crc & 0xFF;												// CRC
	buf[i++] = crc >> 8;
	iap_business(ctx.dev, buf, i);
}

void iap_handle_send_switch_app_cmd(IAPContext& ctx)
{
	uint8_t buf[1 + 2 + 2 + 2] = { 0 };
	uint16_t i = 0;
	buf[i++] = IAP_CMD_SWITCH_APP;						// CMD
	buf[i++] = 2;										// LENGTH
	buf[i++] = 0;
	buf[i++] = ctx.delay & 0xFF;
	buf[i++] = ctx.delay >> 8;
	uint16_t crc = utils::crc16_modbus(buf, i);
	buf[i++] = crc & 0xFF;										// CRC
	buf[i++] = crc >> 8;
	iap_business(ctx.dev, buf, i);
}

void iap_handle_send_write_flash_cmd(IAPContext& ctx)
{
	uint16_t i = 0;
	uint8_t* buf = businessFlashWriteBuf;
	uint8_t* binData = ctx.iap_bin.data() + 128;

	ctx.current_size = (ctx.wBlockIdx == ctx.wBlockNum - 1) ? ctx.wBlockLast : ctx.wBlockSize;
	uint16_t length = ctx.current_size + IAP_PRO_BUSINESS_FLASH_WRITE_ATT_SIZE;
	uint32_t offset = ctx.wBlockIdx * ctx.wBlockSize;

	buf[i++] = IAP_CMD_FLASH_WRITE;
	buf[i++] = length & 0xFF;
	buf[i++] = length >> 8;
	if (ctx.wBlockIdx == ctx.wBlockNum - 1)
	{
		buf[i++] = 0xFF;
		buf[i++] = 0xFF;
	}
	else
	{
		buf[i++] = ctx.wBlockIdx & 0xFF;
		buf[i++] = ctx.wBlockIdx >> 8;
	}
	buf[i++] = offset & 0xFF;
	buf[i++] = offset >> 8;
	buf[i++] = offset >> 16;
	buf[i++] = offset >> 24;
	memcpy(buf + i, binData + (ctx.wBlockIdx * ctx.wBlockSize), ctx.current_size);		// DATA
	i += ctx.current_size;
	uint16_t crc = utils::crc16_modbus(buf, i);
	//if (ctx.wBlockIdx == 10) crc++;
	buf[i++] = crc & 0xFF;
	buf[i++] = crc >> 8;

	iap_business(ctx.dev, buf, i);
}

bool iap_handle(IAPContext& ctx)
{
	printf("IAP status:%d %d %d\n", ctx.primary_status, ctx.process_status, ctx.in_out_status);
	switch (ctx.primary_status)
	{
	case IAP_STATUS_READY:
		ctx.primary_status = IAP_STATUS_RUNNING;
		ctx.process_status = IAP_STATUS_SEND_START_CMD;
		break;
	case IAP_STATUS_RUNNING:
		switch (ctx.in_out_status)
		{
		case IAP_STATUS_SUBSTATUS_WRITE_DATA:
			switch (ctx.process_status)
			{
			case IAP_STATUS_SEND_START_CMD:
				iap_handle_send_start_cmd(ctx);
				break;
			case IAP_STATUS_SEND_WRITE_FLASH:
				iap_handle_send_write_flash_cmd(ctx);
				break;
			case IAP_STATUS_SEND_SWITCH_APP:
				iap_handle_send_switch_app_cmd(ctx);
				break;
			}
			ctx.in_out_status = IAP_STATUS_SUBSTATUS_READ_ACK;
			break;
		case IAP_STATUS_SUBSTATUS_READ_ACK:
			bool timeout = false;
			try {
				iap_business_read(ctx.dev, businessAckBuf, sizeof(businessAckBuf), ctx.readAckTimeout);
			} catch (::timeout_exception) {
				timeout = true;
			}
			
			ctx.ackCode = (iap_business_ack_code_e)businessAckBuf[1];
			if (!timeout && ctx.ackCode == ACK_CODE_SUCCESS)
			{
				ctx.currentRetry = 0;
				ctx.in_out_status = IAP_STATUS_SUBSTATUS_WRITE_DATA;

				if (ctx.process_status == IAP_STATUS_SEND_START_CMD)
				{
					ctx.process_status = IAP_STATUS_SEND_WRITE_FLASH;
				}
				else if (ctx.process_status == IAP_STATUS_SEND_WRITE_FLASH)
				{
					ctx.bin_size_pos += ctx.current_size;
					ctx.wBlockIdx++;
					if (ctx.wBlockIdx == ctx.wBlockNum)
						ctx.process_status = IAP_STATUS_SEND_SWITCH_APP;
				}
				else if (ctx.process_status == IAP_STATUS_SEND_SWITCH_APP)
				{
					ctx.process_status = IAP_STATUS_END;
					ctx.primary_status = IAP_STATUS_COMPLETE;
				}
				ctx.onBusinessSendOk(ctx);
			}
			else
			{
				ctx.in_out_status = IAP_STATUS_SUBSTATUS_WRITE_DATA;
				ctx.currentRetry++;
			}
			if (ctx.currentRetry >= ctx.maximumRetry)
			{
				ctx.terinmateReason = IAP_TERMINATE_REASON_OVER_THE_MAX_RETRY_COUNT;
				ctx.primary_status = IAP_STATUS_TERMINATE;
			}
			break;
		}
		break;
	case IAP_STATUS_TERMINATE:
		return false;
	case IAP_STATUS_COMPLETE:
		return false;
	}
	return true;
}

void iap_run(IAPContext& ctx)
{
	while (!stop && iap_handle(ctx))
	{
	}
}
