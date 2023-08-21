#include "usbdevtree.h"

#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "usb/usb_utils.h"

std::unordered_map<uint32_t, usb::device> deviceMap;


void InitUSBDeviceTree()
{
	deviceMap = usb::enumerate_all_device();
}

//==================================================================================
void ShowUSBDeviceTreeWindow1(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("USB dev tree", p_open, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh Device Tree"))
	{
		deviceMap = usb::enumerate_all_device();
	}


	if (ImGui::TreeNode("My Computer"))
	{
		for (const auto& device : deviceMap)
		{
			if (ImGui::TreeNode(device.second.name.c_str()))
			{
				ImGui::Text(device.second.descriptor_text.c_str());

				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}


	ImGui::End();
}


#define USB_TREE_NODE_ATTR_110(name, value)\
ImGui::TableNextRow();\
ImGui::TableNextColumn();\
ImGui::Text(name);\
ImGui::TableNextColumn();\
ImGui::Text(value);\
ImGui::TableNextColumn();\
ImGui::TextDisabled("--");
#define USB_TREE_NODE_ATTR_111(name, value, desc)\
ImGui::TableNextRow();\
ImGui::TableNextColumn();\
ImGui::Text(name);\
ImGui::TableNextColumn();\
ImGui::Text(value);\
ImGui::TableNextColumn();\
ImGui::Text(desc);

static void ShowEndpointTreeNode(const usb::endpoint_t& endpoint)
{
	std::stringstream ss;
	ss << "endpoint_" << std::hex << (int)endpoint.bEndpointAddress << 'h' << std::dec;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool open = ImGui::TreeNodeEx(ss.str().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::TableNextColumn();
	ImGui::Text("END POINT");
	ImGui::TableNextColumn();
	ImGui::Text((endpoint.bEndpointAddress & 0xF0) ? "IN" : "OUT");

	if (open) {
		ss.str("");
		ss << std::hex << (int)endpoint.bEndpointAddress << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bEndpointAddress", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)endpoint.bmAttributes << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bmAttributes", ss.str().c_str());
		ss.str("");
		ss << (int)endpoint.wMaxPacketSize;
		USB_TREE_NODE_ATTR_110("wMaxPacketSize", ss.str().c_str());

		ImGui::TreePop();
	}
}
static void ShowInterfaceTreeNode(const usb::interface_t& _interface)
{
	std::stringstream ss;
	ss << "interface" << (int)_interface.bInterfaceNumber << '[' << (int)_interface.index << ']';

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool open = ImGui::TreeNodeEx(ss.str().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::TableNextColumn();
	ImGui::Text("INTERFACE");
	ImGui::TableNextColumn();
	ImGui::TextDisabled("--");

	if (open) {

		ss.str("");
		ss << (int)_interface.bInterfaceNumber;
		USB_TREE_NODE_ATTR_110("bInterfaceNumber", ss.str().c_str());
		ss.str("");
		ss << (int)_interface.bAlternateSetting;
		USB_TREE_NODE_ATTR_110("bAlternateSetting", ss.str().c_str());
		ss.str("");
		ss << (int)_interface.bNumEndpoints;
		USB_TREE_NODE_ATTR_110("bNumEndpoints", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)_interface.bInterfaceClass << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bInterfaceClass", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)_interface.bInterfaceSubClass << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bInterfaceSubClass", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)_interface.bInterfaceProtocol << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bInterfaceProtocol", ss.str().c_str());


		for (const auto& endpoint : _interface.endpointList)
			ShowEndpointTreeNode(endpoint);
		ImGui::TreePop();
	}
}
static void ShowConfigurationTreeNode(const usb::configuration_t& configuration)
{
	std::stringstream ss;
	ss << "configuration" << (int)configuration.bConfigurationValue;
	std::string configName = ss.str();

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool open = ImGui::TreeNodeEx(configName.c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::TableNextColumn();
	ImGui::Text("CONFIGURATION");
	ImGui::TableNextColumn();
	ImGui::TextDisabled("--");

	if (open) {
		ss.str("");
		ss << (int)configuration.wTotalLength;
		USB_TREE_NODE_ATTR_110("wTotalLength", ss.str().c_str());
		ss.str("");
		ss << (int)configuration.bConfigurationValue;
		USB_TREE_NODE_ATTR_110("bConfigurationValue", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)configuration.bmAttributes << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bmAttributes", ss.str().c_str());
		ss.str("");
		ss << (int)configuration.bNumInterfaces;
		USB_TREE_NODE_ATTR_110("bNumInterfaces", ss.str().c_str());
		ss.str("");
		ss << (int)configuration.MaxPower;
		USB_TREE_NODE_ATTR_110("MaxPower", ss.str().c_str());

		for (const auto& _interface : configuration.interfaceList)
			ShowInterfaceTreeNode(_interface);
		ImGui::TreePop();
	}
}
static void ShowDeviceTreeNode(const usb::device& device)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;
	if (device.name == "VIDfff1&PIDfa2f" || device.manufacturer == "Ingchips")
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	bool deviceOpen = ImGui::TreeNodeEx(device.name.c_str(), flags);
	ImGui::TableNextColumn();
	if (device.manufacturer == "") ImGui::TextDisabled("--");
	else                           ImGui::Text(device.manufacturer.c_str());
	ImGui::TableNextColumn();
	if (device.product == "")      ImGui::TextDisabled("--");
	else                           ImGui::Text(device.product.c_str());

	if (deviceOpen)
	{
		for (const auto& configuration : device.configurationList)
			ShowConfigurationTreeNode(configuration);

		ImGui::TreePop();
	}
}

void ShowUSBDeviceTreeWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("USB dev tree", p_open, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh Device Tree"))
	{
		deviceMap = usb::enumerate_all_device();
	}


	const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;

	static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

	if (ImGui::BeginTable("usb_device_tree", 3, flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Desc", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		bool open = ImGui::TreeNodeEx("My Window", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
		ImGui::TableNextColumn();
		ImGui::TextDisabled("--");
		ImGui::TableNextColumn();
		ImGui::TextDisabled("--");

		if (open)
		{
			for (const auto& devicePair : deviceMap)
				ShowDeviceTreeNode(devicePair.second);
			ImGui::TreePop();
		}
		ImGui::EndTable();
	}

	ImGui::End();
}
