/*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2019 by Blender Foundation.
* All rights reserved.
*
* Contributor(s): MARUI-PlugIn, Multiplexed Reality
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/intern/vr_network.cpp
*   \ingroup vr
*/

#include "vr_types.h"
#include "vr_main.h"

#include "vr_network.h"

#include "BLI_math.h"

#include "DNA_userdef_types.h"

#include "vr_api.h"

#ifdef WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <process.h>
#else
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#endif
#include <ctime>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/***************************************************************************************************
 * \class										VR_Network
 ***************************************************************************************************
 * VR network module for remote streaming.
 **************************************************************************************************/
 /* Port number used for streaming data. */
#define VR_NETWORK_PORT_NUM	"27010"

char VR_Network::control_sequence[] = { (char)-1, (char)0, (char)-1, (char)0 };
bool VR_Network::initialized(false);
std::atomic<bool> VR_Network::data_new;
std::atomic<bool> VR_Network::img_processed;

char VR_Network::recv_buf[VR_NETWORK_RECV_BUF_SIZE] = { 0 };
char VR_Network::send_buf[VR_NETWORK_SEND_BUF_SIZE] = { 0 };

VR_Network::NetworkStatus VR_Network::network_status(NETWORKSTATUS_INACTIVE);

void *VR_Network::thread(0);
VR_Network::Thread::Runlevel VR_Network::runlvl;
VR_Network::Thread::Condition VR_Network::condition;
void *VR_Network::img_thread(0);
VR_Network::Thread::Runlevel VR_Network::img_runlvl;
VR_Network::Thread::Condition VR_Network::img_condition;

VR_Network::ImageData VR_Network::image_data[VR_SIDES];

std::vector<VR_Network::NetworkAdapter> VR_Network::network_adapters;

bool VR_Network::update_network_adapters()
{
	/* 0: Clear old entries */
	network_adapters.clear();

#ifdef WIN32
	/* 1: Find out how many adapters we have */
	ULONG adapters_size = 0;
	GetAdaptersInfo(0, &adapters_size);
	if (adapters_size == 0) {
		return true;
	}

	/* 2: Get adapter infos */
	IP_ADAPTER_INFO *adapters = new IP_ADAPTER_INFO[adapters_size / sizeof(IP_ADAPTER_INFO)];
	DWORD error = GetAdaptersInfo(adapters, &adapters_size);
	if (error != ERROR_SUCCESS) {
		delete[] adapters;
		return false;
	}

	/* 3: Get names and IP addresses */
	IP_ADAPTER_INFO *adapter = adapters;
	while (adapter) {
		if (adapter->Type != MIB_IF_TYPE_ETHERNET && adapter->Type != IF_TYPE_IEEE80211) {
			adapter = adapter->Next;
			continue;
		}
		if (strncmp(adapter->IpAddressList.IpAddress.String, "0.0.0.0", 8) == 0 || adapter->Description[0] == 0) {
			adapter = adapter->Next;
			continue;
		}
		NetworkAdapter a;
		adapter->Description[sizeof(adapter->Description) - 1] = 0;	/* ensure null-termination */
		a.name = adapter->Description;
		adapter->IpAddressList.IpAddress.String[sizeof(adapter->IpAddressList.IpAddress.String) - 1] = 0;	/* ensure null-termination */
		a.ip_address = adapter->IpAddressList.IpAddress.String;
		network_adapters.push_back(a);	/* add to list */
		adapter = adapter->Next;
	}
	delete[] adapters;
#else
	/* 1: Find out how many adapters we have and
	 * 2: Get adapter infos */
	ifreq ifr;
	ifconf ifc;
	char buf[1024];
	int success = 0;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		return false;
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		return false;
	}

	ifreq *it = ifc.ifc_req;
	const ifreq* const end = it + (ifc.ifc_len / sizeof(ifreq));

	/* 3: Get names and IP addresses */
	for (; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
			if (!(ifr.ifr_flags & IFF_LOOPBACK)) {	/* don't count loopback */
				NetworkAdapter a;
				ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;	/* ensure null-termination */
				a.name = ifr.ifr_name;
				ifr.ifr_addr.sa_data[sizeof(ifr.ifr_addr.sa_data) - 1] = 0;	/* ensure null-termination */
				a.ip_address = ifr.ifr_addr.sa_data;
				network_adapters.push_back(a);	/* add to list */
			}
		}
		else {
			//
		}
	}
#endif
	return true;
}

bool VR_Network::set_image_size(uint width, uint height, uint depth)
{
	if (width == 0 || height == 0 || depth == 0) {
		return false;
	}

	VR_Network::condition.enter();

	/* Allocate bitmap buffer */
	for (int i = 0; i < VR_SIDES; ++i) {
		ImageData& data = VR_Network::image_data[i];
		data.w = width;
		data.h = height;
		data.d = depth;
		if (data.buf) {
			free(data.buf);
		}
		data.compressed_size = 0;
		data.buf = (uchar*)malloc(width * height * depth);
		if (!data.buf) {
			data.w = data.h = data.d = 0;
			VR_Network::condition.leave_silent();
			return false;
		}
	}

	VR_Network::condition.leave_silent();
	return true;
}

bool VR_Network::resample_pixels(const uchar *pixels, uint w_old, uint h_old,
											  uchar *pixels_new, uint w_new, uint h_new, uint depth,
											  const uint *depth_buffer)
{
	if (!pixels || !pixels_new) {
		return false;
	}

	/* Set alpha channel (used for alpha blend) based on depth buffer
	 * to remove viewport background for see-through displays. */
	bool depth_to_alpha = depth_buffer ? true : false;
	uchar *depth_buffer_u8 = depth_buffer ? (uchar*)depth_buffer : NULL;

	int size_old = (int)(w_old * h_old * depth);
	float w_scale = (float)w_new / (float)w_old;
	float h_scale = (float)h_new / (float)h_old;
#pragma omp parallel for
	for (int y = 0; y < (int)h_new; ++y) {
		for (int x = 0; x < (int)w_new; ++x) {
			/* NOTE: Vertical flip */
			int pixel = (y * (w_new * depth) + ((w_new - x) * depth)) - depth;
			int closest_pixel = ((int)((float)y / h_scale) * (w_old * depth)) + ((int)((float)x / w_scale) * depth);
			/* NOTE: Horizontal flip */
			/* RGBA */
			int offset = (size_old - 1) - closest_pixel;
			pixels_new[pixel] = pixels[offset + 1];
			pixels_new[pixel + 1] = pixels[offset + 2];
			pixels_new[pixel + 2] = pixels[offset + 3];
			if (depth_to_alpha) {
				/* Depth buffer is in <depth 24, stencil 8> format */
				uint d_u32 = *(uint*)(&depth_buffer_u8[offset]);
				float d = (d_u32 >> 8) / 16777215.0f;
				if (d == 1.0f) { //|| d < VR_CLIP_NEAR || d > VR_CLIP_FAR) {
					pixels_new[pixel + 3] = 0;
				}
				else {
					pixels_new[pixel + 3] = 255;
				}
			}
			else {
				pixels_new[pixel + 3] = 255;
			}
		}
	}

	return true;
}

bool VR_Network::start()
{
	if (!VR_Network::thread) {
		VR_Network::initialized = false;
		VR_Network::data_new = false;
		VR_Network::img_processed = false;

		/* Intialize image data. */
		if (!VR_Network::set_image_size(320, 240, 4)) { //480, 360, 4)) { //640, 480, 4)) { //1280, 960, 4)) {
			return false;
		}
		VR_Network::image_data[0].quality = VR_Network::image_data[1].quality = 100;

		VR_Network::runlvl = Thread::RUNLEVEL_UNSTARTED;
#ifdef WIN32
		VR_Network::thread = (void *)_beginthreadex(0, 0, &VR_Network::thread_func, 0, 0, NULL);
#else
		VR_Network::thread = (void*)Thread::create(&VR_Network::thread_func);
#endif
		if (!VR_Network::thread) {
			return false;
		}
		/* Wait a moment to see if the thread is actually running */
		VR_Network::condition.enter();
		VR_Network::condition.wait(100);
		VR_Network::condition.leave_silent();
		if (VR_Network::runlvl != Thread::RUNLEVEL_RUNNING) {
			return false;
		}
	}

	if (!VR_Network::img_thread) {
		VR_Network::img_runlvl = Thread::RUNLEVEL_UNSTARTED;
#ifdef WIN32
		VR_Network::img_thread = (void *)_beginthreadex(0, 0, &VR_Network::img_thread_func, 0, 0, NULL);
#else
		VR_Network::img_thread = (void*)Thread::create(&VR_Network::img_thread_func);
#endif
		if (!VR_Network::img_thread) {
			return false;
		}
		/* Wait a moment to see if the thread is actually running */
		VR_Network::img_condition.enter();
		VR_Network::img_condition.wait(100);
		VR_Network::img_condition.leave_silent();
		if (VR_Network::img_runlvl != Thread::RUNLEVEL_RUNNING) {
			return false;
		}
	}

	return true;
}

bool VR_Network::stop()
{
	if (VR_Network::thread) {
		VR_Network::runlvl = Thread::RUNLEVEL_TERMINATING;
		/* Wake the thread and give it some time to terminate */
		VR_Network::condition.enter();
		VR_Network::condition.wait(100);
		VR_Network::condition.leave_silent();
		if (VR_Network::runlvl == Thread::RUNLEVEL_TERMINATING) {
			/* TODO_XR: the thread did not properly terminate - kill it forcefully */
		}
	}

	if (VR_Network::img_thread) {
		VR_Network::img_runlvl = Thread::RUNLEVEL_TERMINATING;
		/* Wake the thread and give it some time to terminate */
		VR_Network::img_condition.enter();
		VR_Network::img_condition.wait(100);
		VR_Network::img_condition.leave_silent();
		if (VR_Network::img_runlvl == Thread::RUNLEVEL_TERMINATING) {
			/* TODO_XR: the thread did not properly terminate - kill it forcefully */
		}
	}

	return true;
}

#ifdef WIN32
bool VR_Network::receive_data(unsigned long long& socket)
{
	const int control_sequence_length = sizeof(VR_Network::control_sequence);
	char control_sequence_buf[control_sequence_length];
	char *control_sequence_buf_ptr = control_sequence_buf;
	int control_bytes_received = 0;
	bool control_sequence_received = false;

	char *recv_buf_ptr = VR_Network::recv_buf;
	int bytes_received = 0;

	clock_t start = clock();

	while (bytes_received < VR_NETWORK_RECV_BUF_SIZE && (clock() - start) < CLOCKS_PER_SEC) {
		int ret;
		if (!control_sequence_received) {
			ret = recv(socket, control_sequence_buf_ptr, control_sequence_length - control_bytes_received, 0);	/* ret receives the number of bytes received, 0, or SOCKET_ERROR */
			if (ret > 0) {
				control_bytes_received += ret;
				if (control_bytes_received < control_sequence_length) {
					control_sequence_buf_ptr += ret;
					continue;
				}
				else if (control_bytes_received > control_sequence_length) {
					return false;
				}
				else { /* control_bytes_received == control_sequence_length  */
					control_sequence_received = true;
					/* TODO_XR: Actually interpret control sequence. */
					continue; /* continue reading to empty the socket */
				}
			}
		}
		else {
			/* Control sequence received, now receive data */
			ret = recv(socket, recv_buf_ptr, VR_NETWORK_RECV_BUF_SIZE - bytes_received, 0);
			if (ret > 0) {
				bytes_received += ret;
				if (bytes_received < VR_NETWORK_RECV_BUF_SIZE) {
					recv_buf_ptr += ret;
				}
				continue; /* continue reading to empty the socket */
			}
		}
		if (ret == 0) {
			return false;
		}
		else if (ret == SOCKET_ERROR) {
			int error = WSAGetLastError();
			switch (error) {
			case WSANOTINITIALISED: { printf("SOCKET ERROR: WSANOTINITIALISED"); return false; }
			case WSAENETDOWN: { printf("SOCKET ERROR: WSAENETDOWN"); return false; }
			case WSAEFAULT: { printf("SOCKET ERROR: WSAEFAULT"); return false; }
			case WSAENOTCONN: { printf("SOCKET ERROR: WSAENOTCONN"); return false; }
			case WSAEINTR: { continue; }
			case WSAEINPROGRESS: { continue; }
			case WSAENETRESET: { printf("SOCKET ERROR: WSAENETRESET"); return false; }
			case WSAENOTSOCK: { printf("SOCKET ERROR: WSAENOTSOCK"); return false; }
			case WSAEOPNOTSUPP: { printf("SOCKET ERROR: WSAEOPNOTSUPP"); return false; }
			case WSAESHUTDOWN: { printf("SOCKET ERROR: WSAESHUTDOWN"); return false; }
			case WSAEWOULDBLOCK: { if (bytes_received == VR_NETWORK_RECV_BUF_SIZE) { return true; } continue; }
			case WSAEMSGSIZE: { continue; }
			case WSAEINVAL: { printf("SOCKET ERROR: WSAEINVAL"); return false; }
			case WSAECONNABORTED: { printf("SOCKET ERROR: WSAECONNABORTED"); return false; }
			case WSAETIMEDOUT: { printf("SOCKET ERROR: WSAETIMEDOUT"); return false; }
			case WSAECONNRESET: { printf("SOCKET ERROR: WSAECONNRESET"); return false; }
			}
			/* else: undefined error */
			printf("SOCKET ERROR: UNDEFINED ERROR");
			return false;
		}
	}

	if (bytes_received == VR_NETWORK_RECV_BUF_SIZE) {
		return true;
	}
	else {
		printf("SOCKET ERROR: TIMEOUT");
		return false;	/* timeout occurred */
	}
}
#else
bool VR_Network::receive_data(int& socket)
{
	const int control_sequence_length = sizeof(VR_Network::control_sequence);
	char control_sequence_buf[control_sequence_length];
	char *control_sequence_buf_ptr = control_sequence_buf;
	int control_bytes_received = 0;
	bool control_sequence_received = false;

	char *recv_buf_ptr = VR_Network::recv_buf;
	int bytes_received = 0;

	clock_t start = clock();

	while (bytes_received < VR_NETWORK_RECV_BUF_SIZE && (clock() - start) < CLOCKS_PER_SEC) {
		int ret;
		if (!control_sequence_received) {
			ret = recv(socket, control_sequence_buf_ptr, control_sequence_length - control_bytes_received, 0);	/* ret receives the number of bytes received, 0, or SOCKET_ERROR */
			if (ret > 0) {
				control_bytes_received += ret;
				if (control_bytes_received < control_sequence_length) {
					control_sequence_buf_ptr += ret;
					continue;
				}
				else if (control_bytes_received > control_sequence_length) {
					return false;
				}
				else { /* control_bytes_received == control_sequence_length  */
					control_sequence_received = true;
					/* TODO_XR: Actually interpret control sequence. */
					continue; /* continue reading to empty the socket */
				}
			}
		}
		else {
			/* Control sequence received, now receive data */
			ret = recv(socket, recv_buf_ptr, VR_NETWORK_RECV_BUF_SIZE - bytes_received, 0);
			if (ret > 0) {
				bytes_received += ret;
				if (bytes_received < VR_NETWORK_RECV_BUF_SIZE) {
					recv_buf_ptr += ret;
				}
				continue; /* continue reading to empty the socket */
			}
		}
		if (ret == 0) {
			return false;
		}
		else if (ret == -1) {
			int error = errno;
			switch (error) {
			case ENETDOWN: { printf("SOCKET ERROR: ENETDOWN"); return false; }
			case EFAULT: { printf("SOCKET ERROR: EFAULT"); return false; }
			case ENOTCONN: { printf("SOCKET ERROR: ENOTCONN"); return false; }
			case EINTR: { continue; }
			case EINPROGRESS: { continue; }
			case ENETRESET: { printf("SOCKET ERROR: ENETRESET"); return false; }
			case ENOTSOCK: { printf("SOCKET ERROR: ENOTSOCK"); return false; }
			case EOPNOTSUPP: { printf("SOCKET ERROR: EOPNOTSUPP"); return false; }
			case ESHUTDOWN: { printf("SOCKET ERROR: ESHUTDOWN"); return false; }
			case EWOULDBLOCK: { if (bytes_received == VR_NETWORK_RECV_BUF_SIZE) { return true; } continue; }
			case EMSGSIZE: { continue; }
			case EINVAL: { printf("SOCKET ERROR: EINVAL"); return false; }
			case ECONNABORTED: { printf("SOCKET ERROR: ECONNABORTED"); return false; }
			case ETIMEDOUT: { printf("SOCKET ERROR: ETIMEDOUT"); return false; }
			case ECONNRESET: { printf("SOCKET ERROR: ECONNRESET"); return false; }
			}
			/* else: undefined error */
			printf("SOCKET ERROR: UNDEFINED ERROR");
			return false;
		}
	}

	if (bytes_received == VR_NETWORK_RECV_BUF_SIZE) {
		return true;
	}
	else {
		printf("SOCKET ERROR: TIMEOUT");
		return false; /* timeout occurred */
	}
}
#endif

#ifdef WIN32
bool VR_Network::send_data(unsigned long long& socket)
{
	const int control_sequence_length = sizeof(VR_Network::control_sequence);
	char *control_sequence_buf_ptr = VR_Network::control_sequence;
	bool control_sequence_sent = false;
	uint control_bytes_sent = 0;

	static char local_buf[VR_NETWORK_SEND_BUF_SIZE];
	static uint size_l = 0;
	static uint size_r = 0;
	if (VR_Network::img_processed) {
		size_l = VR_Network::image_data[VR_SIDE_LEFT].compressed_size;
		size_r = VR_Network::image_data[VR_SIDE_RIGHT].compressed_size;
		memcpy(local_buf, VR_Network::send_buf, size_l);
		memcpy(&local_buf[size_l], &VR_Network::send_buf[size_l], size_r);
		VR_Network::img_processed = false;
	}

	const int size_sequence_length_l = 4;
	char *size_sequence_buf_ptr_l = (char*)&size_l;
	bool size_sequence_sent_l = false;
	uint size_bytes_sent_l = 0;

	const int size_sequence_length_r = 4;
	char *size_sequence_buf_ptr_r = (char*)&size_r;
	bool size_sequence_sent_r = false;
	uint size_bytes_sent_r = 0;

#if VR_NETWORK_IMAGE_STREAMING
	const uint send_buf_size = size_l + size_r;
#else
	const uint send_buf_size = 4;
#endif
	char *send_buf_ptr = local_buf;
	uint bytes_sent = 0;

	clock_t start = clock();

	while (bytes_sent < send_buf_size && (clock() - start) < CLOCKS_PER_SEC) {
		int ret;
		if (!control_sequence_sent) {
			/* First try to send control sequence */
			ret = send(socket, control_sequence_buf_ptr, control_sequence_length - control_bytes_sent, 0); /* ret receives the number of bytes sent, 0, or SOCKET_ERROR */
			if (ret > 0) {
				control_bytes_sent += ret;
				if (control_bytes_sent < control_sequence_length) {
					control_sequence_buf_ptr += ret;
					continue;
				}
				else if (control_bytes_sent > control_sequence_length) {
					return false;
				}
				else { /* control_bytes_sent == control_sequence_length */
					control_sequence_sent = true;
#if VR_NETWORK_IMAGE_STREAMING
					continue; /* continue reading to empty the socket */
#else
					return true;
#endif
				}
			}
		}
		else if (!size_sequence_sent_l) {
			/* Next try to send size sequence (left) */
			ret = send(socket, size_sequence_buf_ptr_l, size_sequence_length_l - size_bytes_sent_l, 0);
			if (ret > 0) {
				size_bytes_sent_l += ret;
				if (size_bytes_sent_l < size_sequence_length_l) {
					size_sequence_buf_ptr_l += ret;
					continue;
				}
				else if (size_bytes_sent_l > size_sequence_length_l) {
					return false;
				}
				else { /* size_bytes_sent_l == size_sequence_length_l */
					size_sequence_sent_l = true;
					continue; /* continue reading to empty the socket */
				}
			}
		}
		else if (!size_sequence_sent_r) {
			/* Next try to send size sequence (right) */
			ret = send(socket, size_sequence_buf_ptr_r, size_sequence_length_r - size_bytes_sent_r, 0);
			if (ret > 0) {
				size_bytes_sent_r += ret;
				if (size_bytes_sent_r < size_sequence_length_r) {
					size_sequence_buf_ptr_r += ret;
					continue;
				}
				else if (size_bytes_sent_r > size_sequence_length_r) {
					return false;
				}
				else { /* size_bytes_sent_r == size_sequence_length_r */
					size_sequence_sent_r = true;
					continue; /* continue reading to empty the socket */
				}
			}
		}
		else {
			/* Size sequence sent, now send data */
			ret = send(socket, send_buf_ptr, send_buf_size - bytes_sent, 0);
			if (ret > 0) {
				bytes_sent += ret;
				if (bytes_sent < send_buf_size) {
					send_buf_ptr += ret;
				}
				continue; /* continue sending */
			}
		}
		if (ret == 0) {
			return false;	/* ret == 0 means host closed connection */
		}
		else if (ret == SOCKET_ERROR) {
			int error = WSAGetLastError();
			switch (error) {
			case WSANOTINITIALISED: { printf("SOCKET ERROR: WSANOTINITIALISED"); return false; }
			case WSAENETDOWN: { printf("SOCKET ERROR: WSAENETDOWN"); return false; }
			case WSAEFAULT: { printf("SOCKET ERROR: WSAEFAULT"); return false; }
			case WSAENOTCONN: { printf("SOCKET ERROR: WSAENOTCONN"); return false; }
			case WSAEINTR: { continue; }
			case WSAEINPROGRESS: { continue; }
			case WSAENETRESET: { printf("SOCKET ERROR: WSAENETRESET"); return false; }
			case WSAENOTSOCK: { printf("SOCKET ERROR: WSAENOTSOCK"); return false; }
			case WSAEOPNOTSUPP: { printf("SOCKET ERROR: WSAEOPNOTSUPP"); return false; }
			case WSAESHUTDOWN: { printf("SOCKET ERROR: WSAESHUTDOWN"); return false; }
			case WSAEWOULDBLOCK: { if (bytes_sent == send_buf_size) { return true; } continue; }
			case WSAEMSGSIZE: { continue; }
			case WSAEINVAL: { printf("SOCKET ERROR: WSAEINVAL"); return false; }
			case WSAECONNABORTED: { printf("SOCKET ERROR: WSAECONNABORTED"); return false; }
			case WSAETIMEDOUT: { printf("SOCKET ERROR: WSAETIMEDOUT"); return false; }
			case WSAECONNRESET: { printf("SOCKET ERROR: WSAECONNRESET"); return false; }
			}
			/* else: undefined error */
			printf("SOCKET ERROR: UNDEFINED ERROR");
			return false;
		}
	}

	if (bytes_sent == send_buf_size) {
		return true;
	}
	else {
		printf("SOCKET ERROR: TIMEOUT");
		return false; /* timeout occurred */
	}
}
#else
bool VR_Network::send_data(int& socket)
{
	const int control_sequence_length = sizeof(VR_Network::control_sequence);
	char *control_sequence_buf_ptr = VR_Network::control_sequence;
	bool control_sequence_sent = false;
	int control_bytes_sent = 0;

	const int size_sequence_length_l = 4;
	char *size_sequence_buf_ptr_l = (char*)& VR_Network::image_data[VR_SIDE_LEFT].compressed_size;
	bool size_sequence_sent_l = false;
	int size_bytes_sent_l = 0;

	const int size_sequence_length_r = 4;
	char *size_sequence_buf_ptr_r = (char*)& VR_Network::image_data[VR_SIDE_RIGHT].compressed_size;
	bool size_sequence_sent_r = false;
	int size_bytes_sent_r = 0;

#if VR_NETWORK_IMAGE_STREAMING
	const int send_buf_size = VR_Network::image_data[VR_SIDE_LEFT].compressed_size +
							  VR_Network::image_data[VR_SIDE_RIGHT].compressed_size;
#else
	const int send_buf_size = 4;
#endif
	char *send_buf_ptr = VR_Network::send_buf;
	int bytes_sent = 0;

	clock_t start = clock();

	while (bytes_sent < send_buf_size && (clock() - start) < CLOCKS_PER_SEC) {
		int ret;
		if (!control_sequence_sent) {
			/* First try to send control sequence */
			ret = send(socket, control_sequence_buf_ptr, control_sequence_length - control_bytes_sent, 0); /* ret receives the number of bytes sent, 0, or SOCKET_ERROR */
			if (ret > 0) {
				control_bytes_sent += ret;
				if (control_bytes_sent < control_sequence_length) {
					control_sequence_buf_ptr += ret;
					continue;
				}
				else if (control_bytes_sent > control_sequence_length) {
					return false;
				}
				else { /* control_bytes_sent == control_sequence_length */
					control_sequence_sent = true;
#if VR_NETWORK_IMAGE_STREAMING
					continue; /* continue reading to empty the socket */
#else
					return true;
#endif
				}
			}
		}
		else if (!size_sequence_sent_l) {
			/* Next try to send size sequence (left) */
			ret = send(socket, size_sequence_buf_ptr_l, size_sequence_length_l - size_bytes_sent_l, 0);
			if (ret > 0) {
				size_bytes_sent_l += ret;
				if (size_bytes_sent_l < size_sequence_length_l) {
					size_sequence_buf_ptr_l += ret;
					continue;
				}
				else if (size_bytes_sent_l > size_sequence_length_l) {
					return false;
				}
				else { /* size_bytes_sent_l == size_sequence_length_l */
					size_sequence_sent_l = true;
					continue; /* continue reading to empty the socket */
				}
			}
		}
		else if (!size_sequence_sent_r) {
			/* Next try to send size sequence (right) */
			ret = send(socket, size_sequence_buf_ptr_r, size_sequence_length_r - size_bytes_sent_r, 0);
			if (ret > 0) {
				size_bytes_sent_r += ret;
				if (size_bytes_sent_r < size_sequence_length_r) {
					size_sequence_buf_ptr_r += ret;
					continue;
				}
				else if (size_bytes_sent_r > size_sequence_length_r) {
					return false;
				}
				else { /* size_bytes_sent_r == size_sequence_length_r */
					size_sequence_sent_r = true;
					continue; /* continue reading to empty the socket */
				}
			}
		}
		else {
			/* Control sequence sent, now send data */
			ret = send(socket, send_buf_ptr, send_buf_size- bytes_sent, 0);
			if (ret > 0) {
				bytes_sent += ret;
				if (bytes_sent < send_buf_size) {
					send_buf_ptr += ret;
				}
				continue; /* continue sending */
			}
		}
		if (ret == 0) {
			return false;	/* ret == 0 means host closed connection */
		}
		else if (ret == -1) {
			int error = errno;
			switch (error) {
			case ENETDOWN: { printf("SOCKET ERROR: ENETDOWN"); return false; }
			case EFAULT: { printf("SOCKET ERROR: EFAULT"); return false; }
			case ENOTCONN: { printf("SOCKET ERROR: ENOTCONN"); return false; }
			case EINTR: { continue; }
			case EINPROGRESS: { continue; }
			case ENETRESET: { printf("SOCKET ERROR: ENETRESET"); return false; }
			case ENOTSOCK: { printf("SOCKET ERROR: ENOTSOCK"); return false; }
			case EOPNOTSUPP: { printf("SOCKET ERROR: EOPNOTSUPP"); return false; }
			case ESHUTDOWN: { printf("SOCKET ERROR: ESHUTDOWN"); return false; }
			case EWOULDBLOCK: { if (bytes_sent == send_buf_size) { return true; } continue; }
			case EMSGSIZE: { continue; }
			case EINVAL: { printf("SOCKET ERROR: EINVAL"); return false; }
			case ECONNABORTED: { printf("SOCKET ERROR: ECONNABORTED"); return false; }
			case ETIMEDOUT: { printf("SOCKET ERROR: ETIMEDOUT"); return false; }
			case ECONNRESET: { printf("SOCKET ERROR: ECONNRESET"); return false; }
			}
			/* else: undefined error */
			printf("SOCKET ERROR: UNDEFINED ERROR");
			return false;
		}
	}

	if (bytes_sent == send_buf_size) {
		return true;
	}
	else {
		printf("SOCKET ERROR: TIMEOUT");
		return false; /* timeout occurred */
	}
}
#endif

#ifdef WIN32
uint __stdcall VR_Network::thread_func(void *data)
{
	VR_Network::runlvl = Thread::RUNLEVEL_RUNNING;
	VR_Network::network_status = NETWORKSTATUS_NOTCONNECTED;
	/* Wait for the creator thread to resume and notice that the current thread is running. */
	VR_Network::condition.enter();
	VR_Network::condition.wait(50);
	VR_Network::condition.leave_signal();

	WSADATA wsa_data;
	struct addrinfo *result = NULL;
	struct addrinfo hints;

	/* Initialize networking */
	int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (ret != 0) {
		//printf("WSAStartup failed");
		return 1;
	}
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	while (VR_Network::runlvl == Thread::RUNLEVEL_RUNNING) {
		if (std::strlen(U.vr_network_ipaddr) == 0) {
			VR_Network::network_status = NETWORKSTATUS_NOTCONNECTED;
			Sleep(1000);
			continue;
		}

		/* Try to connect to given IP address */
		VR_Network::network_status = NETWORKSTATUS_STARTINGNETWORK;
		PCSTR ip_addr = U.vr_network_ipaddr;
		ret = getaddrinfo(ip_addr, VR_NETWORK_PORT_NUM, &hints, &result);
		if (ret != 0) {
			//printf("Failed to get address info.");
			continue;
		}
		SOCKET listen_socket = INVALID_SOCKET;
		listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (listen_socket == INVALID_SOCKET) {
			//printf("Failed to initialize network connection.");
			continue;
		}

		/* Host: Bind to socket */
		ret = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
		int error;
		if ((ret == SOCKET_ERROR) && ((error = WSAGetLastError()) != WSAEADDRINUSE)) {
			//printf("Failed to establish network connection.");
			continue;
		}
		freeaddrinfo(result);

		/* Set listen socket to be non-blocking */
		u_long mode = 1;
		ioctlsocket(listen_socket, FIONBIO, &mode);
		SOCKET client_socket = INVALID_SOCKET;

		/* If we arrive here, we successfully created a socket */
		std::string current_ip_address = U.vr_network_ipaddr;	/* the (local) IP address that we are currently using */
		VR_Network::network_status = NETWORKSTATUS_WAITINGFORCLIENT;
		bool connected = false;

		/* Host: Listen for incoming connections */
		while (VR_Network::runlvl == Thread::RUNLEVEL_RUNNING) {
			/* If IP selection changed, go back to ip address loop */
			if (current_ip_address != U.vr_network_ipaddr) {
				break;
			}
			/* Listen on socket */
			if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
				//printf("Listen failed.");
				break;
			}
			client_socket = accept(listen_socket, (sockaddr*)NULL, NULL);
			if (client_socket == INVALID_SOCKET) {
				/* no client connected yet */
				Sleep(1000);
				continue;
			}
			/* else: client connected */
			connected = true;
			closesocket(listen_socket);
			listen_socket = INVALID_SOCKET;
			break;
		}
		if (!connected) {
			closesocket(listen_socket);
			continue;
		}

		/* If we arrive here, the client successfully connected */
		VR_Network::network_status = NETWORKSTATUS_CONNECTED;

		/* Enter the "wait-for-request-and-send-data" loop */
		while (VR_Network::runlvl == Thread::RUNLEVEL_RUNNING && current_ip_address == U.vr_network_ipaddr) {
			if (!receive_data(client_socket)) {
				break; /*some problem receiving the data (or: timeout) close and re-connect */
			}
			/* else: received a request */
			if (!VR_Network::initialized) {
				VR_Network::initialized = true;
			}

			/* Get VR data and send to client */
			VR_Network::condition.enter();	/* lock the data buffer */

#if VR_NETWORK_IMAGE_STREAMING
			if (!VR_Network::img_processed) { /* wait a bit for the next update */
				VR_Network::condition.wait(100);
				if (!VR_Network::img_processed) {
					//printf("timeout waiting for new data");
				}
			}
#else
			if (!VR_Network::data_new) { /* wait a bit for the next update */
				VR_Network::condition.wait(100);
				if (!VR_Network::data_new) {
					//printf("timeout waiting for new data");
				}
			}
			if (VR_Network::data_new) {
				VR_Network::data_new = false;
			}
#endif

			VR_Network::condition.leave_signal();	/* finished using the data buffer */

			/* Send data */
			if (!send_data(client_socket)) {
				break; /*some problem sending the data (or: timeout) close and re-connect */
			}
		}
		/* if we arrive here, either the user changed the IP or we lost the connection */
		VR_Network::network_status = NETWORKSTATUS_DISCONNECT;
		shutdown(client_socket, SD_SEND);
		closesocket(client_socket);
		if (listen_socket != INVALID_SOCKET) {
			closesocket(listen_socket);
		}
		/* continue while runlevel allows it */
	}

	/* If we arrive here, runlevel was set to false */
	VR_Network::network_status = NETWORKSTATUS_NETWORKSHUTDOWN;
	WSACleanup();
	VR_Network::network_status = NETWORKSTATUS_INACTIVE;
	VR_Network::thread = 0;

	return 0;
}
#else
void sigchld_handler(int s) {
	while (wait(NULL) > 0);
}

void VR_Network::thread_func()
{
	VR_Network::runlvl = Thread::RUNLEVEL_RUNNING;
	VR_Network::network_status = NETWORKSTATUS_NOTCONNECTED;
	/* Wait for the creator thread to resume and notice that the current thread is running. */
	VR_Network::condition.enter();
	VR_Network::condition.wait(50);
	VR_Network::condition.leave_signal();

	int listen_socket, client_socket;

	struct sockaddr_in host_addr;
	struct sockaddr_in client_addr;
	int sin_size = sizeof(struct sockaddr_in);
	struct sigaction sa;

	int yes = 1;

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons((u_short)atoi(VR_NETWORK_PORT_NUM));
	memset(&(host_addr.sin_zero), '\0', 8);

	while (VR_Network::runlvl == Thread::RUNLEVEL_RUNNING) {
		if (std::strlen(U.vr_network_ipaddr) == 0) {
			VR_Network::network_status = NETWORKSTATUS_NOTCONNECTED;
			/* TODO: Make it so we don't have to call Sleep()? (notification upon change) */
			usleep(1000000);
			continue;
		}

		/* Try to connect to given IP address */
		VR_Network::network_status = NETWORKSTATUS_STARTINGNETWORK;
		host_addr.sin_addr.s_addr = inet_addr(U.vr_network_ipaddr);
		if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			//printf("Server-socket() error.");
			continue;
		}
		if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			//printf("Server-setsockopt() error.");
			continue;
		}

		/* Host: Bind to socket. */
		if (bind(listen_socket, (struct sockaddr*) & host_addr, sizeof(struct sockaddr)) == -1) {
			//printf("Server-bind() error.");
			continue;
		}

		/* Clean all the dead processes */
		sa.sa_handler = sigchld_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;

		if (sigaction(SIGCHLD, &sa, NULL) == -1) {
			//printf("Server-sigaction() error.");
			continue;
		}

		/* Set listen socket to be non-blocking */
		int flags = fcntl(listen_socket, F_GETFL);
		fcntl(listen_socket, F_SETFL, flags | O_NONBLOCK);

		/* If we arrive here, we successfully created a socket */
		std::string current_ip_address = U.vr_network_ipaddr;	/* the (local) IP address that we are currently using */
		VR_Network::network_status = NETWORKSTATUS_WAITINGFORCLIENT;
		bool connected = false;

		/* Host: Listen for incoming connections */
		while (VR_Network::runlvl == Thread::RUNLEVEL_RUNNING) {
			/* If ip selection changed, go back to ip address loop */
			if (current_ip_address != U.vr_network_ipaddr) {
				break;
			}
			if (listen(listen_socket, SOMAXCONN) == -1) {
				//printf("Server-listen() error.");
				break;
			}
			/* Listen on socket */
			sin_size = sizeof(struct sockaddr_in);
			if ((client_socket = accept(listen_socket, (struct sockaddr*) &client_addr, (socklen_t*)&sin_size)) == -1) {
				//printf("Server-accept() error.");
				usleep(1000000);
				continue;
			}
			/* This is the child process */
			//if (!fork()) {
			//	/* Child doesn't need the listener */
			//	close(listen_socket);
			//}
			/* else: client connected */
			connected = true;
			close(listen_socket);
			listen_socket = 0;
			break;
		}
		if (!connected) {
			close(listen_socket);
			continue;
		}

		/* If we arrive here, the client successfully connected */
		VR_Network::network_status = NETWORKSTATUS_CONNECTED;

		/* Enter the "wait-for-request-and-send-data" loop */
		while (VR_Network::runlvl == Thread::RUNLEVEL_RUNNING && current_ip_address == U.vr_network_ipaddr) {
			if (!receive_data(client_socket)) {
				break;	/* some problem receiving the data (or: timeout) close and re-connect */
			}
			/* else: received a request */
			if (!VR_Network::initialized) {
				VR_Network::initialized = true;
			}

			/* Get VR data and send to client */
			VR_Network::condition.enter();	/* lock the data buffer */

#if VR_NETWORK_IMAGE_STREAMING
			if (!VR_Network::img_processed) { /* wait a bit for the next update */
				VR_Network::condition.wait(100);
				if (!VR_Network::img_processed) {
					//printf("timeout waiting for new data");
				}
			}
#else
			if (!VR_Network::data_new) { /* wait a bit for the next update */
				VR_Network::condition.wait(100);
				if (!VR_Network::data_new) {
					//printf("timeout waiting for new data");
				}
			}
			if (VR_Network::data_new) {
				VR_Network::data_new = false;
			}
#endif

			VR_Network::condition.leave_signal();	/* finished using the data buffer */

			/* Send data */
			if (!send_data(client_socket)) {
				break; /*some problem sending the data (or: timeout) close and re-connect */
			}
		}
		/* if we arrive here, either the user changed the IP or we lost the connection */
		VR_Network::network_status = NETWORKSTATUS_DISCONNECT;
		close(client_socket);
		if (listen_socket != 0) {
			close(listen_socket);
		}
		/* continue while runlevel allows it */
	}

	/* If we arrive here, runlevel was set to false */
	VR_Network::network_status = NETWORKSTATUS_NETWORKSHUTDOWN;
	VR_Network::network_status = NETWORKSTATUS_INACTIVE;
	VR_Network::thread = 0;

	return;
}
#endif

#ifdef WIN32
uint __stdcall VR_Network::img_thread_func(void *data)
#else
void VR_Network::img_thread_func()
#endif
{
	VR_Network::img_runlvl = Thread::RUNLEVEL_RUNNING;
	/* Wait for the creator thread to resume and notice that the current thread is running. */
	VR_Network::img_condition.enter();
	VR_Network::img_condition.wait(50);
	VR_Network::img_condition.leave_signal();

	while (VR_Network::img_runlvl == Thread::RUNLEVEL_RUNNING) {
		if (!VR_Network::data_new) {
#ifdef WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
			continue;
		}

		bool error[VR_SIDES] = { false, false };
		uchar *out[VR_SIDES] = { 0, 0 };
		int size[VR_SIDES] = { 0, 0 };

#pragma omp parallel for
		for (int i = 0; i < VR_SIDES; ++i) {
			ImageData& data = VR_Network::image_data[i];
			out[i] = stbi_zlib_compress(data.buf, VR_NETWORK_SEND_BUF_SIZE_HALF, &size[i], data.quality);
		}
		if (error[VR_SIDE_LEFT] || error[VR_SIDE_RIGHT] ||
			(size[VR_SIDE_LEFT] + size[VR_SIDE_RIGHT] > VR_NETWORK_SEND_BUF_SIZE)) {
			goto loop_end;
		}
		if (!VR_Network::img_processed) {
			ImageData& data_left = VR_Network::image_data[VR_SIDE_LEFT];
			ImageData& data_right = VR_Network::image_data[VR_SIDE_RIGHT];
			data_left.compressed_size = (uint)size[VR_SIDE_LEFT];
			data_right.compressed_size = (uint)size[VR_SIDE_RIGHT];
			memcpy(&VR_Network::send_buf, out[VR_SIDE_LEFT], data_left.compressed_size);
			memcpy(&VR_Network::send_buf[data_left.compressed_size], out[VR_SIDE_RIGHT], data_right.compressed_size);
			VR_Network::img_processed = true;
		}

loop_end:
		for (int i = 0; i < VR_SIDES; ++i) {
			if (out[i]) {
				free(out[i]);
			}
		}

		VR_Network::data_new = false;
	}

	/* If we arrive here, runlevel was set to false */
	VR_Network::img_thread = 0;
#ifdef WIN32
	return 0;
#else
	return;
#endif
}

/***************************************************************************************************
 * \class                                   VR_Network::Thread
 ***************************************************************************************************
 * This object implements threading logic in a platform independent manner.
 * The class also features several common utilities for parallelization and synchronization.
 **************************************************************************************************/

#ifdef WIN32
/* Windows bounce function for thread creation without parameter ("void thread_function()").
 * Simply calls the function pointer and, when the thread returns, returns NULL.
 * \param   lpParameter     Pointer to thread function. Expected to be a void function pointer.
 * \return                  NULL. */
DWORD WINAPI windows_thread_proc(LPVOID lp_param)
{
	void_func_ptr f = (void_func_ptr)lp_param;
	(*f)();
	return NULL;
}

/* Primitive struct to contain both the function pointer and the parameter pointer,
 * because I can pass only one pointer to the thread function. */
struct WindowsThreadProcPointers
{
	voidptr_func_ptr func;
	void *param;
};

/* Windows bounce function for thread creation with parameter ("void thread_function(void* param)").
 * Expects the param to be a pointer to an array of two void pointers:
 * where the first one is the function pointer, and the second one is the parameter pointer.
 * The array will be deleted after the pointers are retrieved.
 * Then it calls the function pointer and, when the thread returns, returns NULL.
 * \param   lpParameter     Pointer to a WindowsThreadProcPointers struct, that contains both the
 *                          function pointer and the parameter pointer. Will be deleted immediately.
 * \return                  NULL. */
DWORD WINAPI windows_thread_proc2(LPVOID lp_param)
{
	WindowsThreadProcPointers *pointers = (WindowsThreadProcPointers*)lp_param;
	voidptr_func_ptr func = pointers->func;
	void *param = pointers->param;
	delete pointers;
	(*func)(param);
	return NULL;
}

#endif

struct DelayedCallStruct {
	uint delay;
	voidptr_func_ptr func;
	void *param;
};

#ifdef WIN32
DWORD WINAPI thread_proc_delayed_call(LPVOID lp_param)
{
	DelayedCallStruct* delayed_call_struct = (DelayedCallStruct*)lp_param;
	Sleep(delayed_call_struct->delay);
	delayed_call_struct->func(delayed_call_struct->param);
	delete delayed_call_struct;
	return NULL;
}
#else
void thread_proc_delayed_call(void *param)
{
	DelayedCallStruct *delayed_call_struct = (DelayedCallStruct*)param;
	std::this_thread::sleep_for(std::chrono::milliseconds(delayed_call_struct->delay));
	delayed_call_struct->func(delayed_call_struct->param);
	delete delayed_call_struct;
}
#endif

bool VR_Network::Thread::delayed_call(uint ms, voidptr_func_ptr function, void *param)
{
	DelayedCallStruct *delayed_call_struct = new DelayedCallStruct{ ms, function, param };
#ifdef WIN32
	DWORD id;
	HANDLE thread_handle = CreateThread(NULL, 0, thread_proc_delayed_call, (LPVOID)delayed_call_struct, NULL, &id);
#else
	std::thread *thread_handle = new std::thread(thread_proc_delayed_call, delayed_call_struct);
#endif
	if (thread_handle == NULL) {
		printf("VR_Network::Thread : Failed to create thread.");
		return false;
	}

	return true;
}

VR_Network::Thread *VR_Network::Thread::create(void_func_ptr f)
{
#ifdef WIN32
	DWORD id;
	HANDLE thread_handle = CreateThread(NULL, 0, windows_thread_proc, (LPVOID)f, NULL, &id);
#else
	std::thread *thread_handle = new std::thread(f);
#endif
	if (thread_handle == NULL) {
		printf("VR_Network::Thread : Failed to create thread.");
		return NULL;
	}

	Thread *thread = new Thread(thread_handle);
	return thread;
}

VR_Network::Thread *VR_Network::Thread::create(voidptr_func_ptr f, void *param)
{
#ifdef WIN32
	/* Prepare array of pointers */
	WindowsThreadProcPointers *pointers = new WindowsThreadProcPointers();
	pointers->func = f;
	pointers->param = param;
	DWORD id;
	HANDLE thread_handle = CreateThread(NULL, 0, windows_thread_proc2, (LPVOID)pointers, NULL, &id);

	if (thread_handle == NULL) {
		printf("VR_Network::Thread : Failed to create thread.");
		return NULL;
	}
#else
	std::thread *thread_handle = new std::thread(f, param);
	if (!thread_handle || !thread_handle->joinable()) {
		printf("VR_Network::Thread : Failed to create thread.");
		return NULL;
	}
#endif

	Thread *thread = new Thread(thread_handle);
	return thread;
}

bool VR_Network::Thread::destroy(Thread *thread)
{
#ifdef WIN32
	if (TerminateThread((HANDLE)thread->thread_ident, (DWORD)-1) == 0) {
		printf("VR_Network::Thread : Failed to destroy thread.");
		return false;
	}
#else
	/* Can't manually destroy C++ threads?
	 * Waiting / joining just risks that the caller thread also blocks. */
#endif

	delete thread;
	return true;
}

VR_Network::Thread::Thread(void *ident)
{
	this->thread_ident = ident;
}

VR_Network::Thread::~Thread()
{
	//
}

const void *VR_Network::Thread::id()
{
	return this->thread_ident;
}

void VR_Network::Thread::sleep(uint ms)
{
#ifdef WIN32
	Sleep((DWORD)ms);
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

/***************************************************************************************************
 * \class                               Thread::Condition
 ***************************************************************************************************
 * This object is a platform independent wrapper for Condition Variables.
 * Condition variables are mainly critical sections that allow waiting inside the critical
 * section (without blocking other threads in the meantime) and notifying those waiting threads
 * when leaving the critical section.
 **************************************************************************************************/

VR_Network::Thread::Condition::Condition()
{
#ifdef WIN32
	CONDITION_VARIABLE *condition_variable = new CONDITION_VARIABLE();
	CRITICAL_SECTION *critical_section = new CRITICAL_SECTION();
	InitializeConditionVariable(condition_variable);
	InitializeCriticalSection(critical_section);
	this->condition_ident = (void*)condition_variable;
	this->mutex_ident = (void*)critical_section;
#else
	this->mutex_ident = new std::mutex;
	this->condition_ident = new std::condition_variable;
#endif
}

VR_Network::Thread::Condition::~Condition()
{
#ifdef WIN32
	CONDITION_VARIABLE *condition_variable = (CONDITION_VARIABLE*)this->condition_ident;
	CRITICAL_SECTION *critical_section = (CRITICAL_SECTION*)this->mutex_ident;
	delete condition_variable;
	delete critical_section;
#else
	std::mutex *m = (std::mutex*)this->mutex_ident;
	std::condition_variable *cv = (std::condition_variable*)this->condition_ident;
	delete m;
	delete cv;
#endif
}

void VR_Network::Thread::Condition::enter()
{
#ifdef WIN32
	EnterCriticalSection((CRITICAL_SECTION*)this->mutex_ident);
#else
	((std::mutex*)this->mutex_ident)->lock();
#endif
}

void VR_Network::Thread::Condition::wait()
{
#ifdef WIN32
	SleepConditionVariableCS((CONDITION_VARIABLE*)this->condition_ident,(CRITICAL_SECTION*)this->mutex_ident, INFINITE);
#else
	std::unique_lock<std::mutex> lock(*(std::mutex*)mutex_ident, std::adopt_lock);
	((std::condition_variable*)this->condition_ident)->wait(lock);
	lock.release();
#endif
}

bool VR_Network::Thread::Condition::wait(uint ms)
{
#ifdef WIN32
	return SleepConditionVariableCS((CONDITION_VARIABLE*)this->condition_ident, (CRITICAL_SECTION*)this->mutex_ident, ms) != 0;
#else
	std::unique_lock<std::mutex> lock(*(std::mutex*)this->mutex_ident, std::adopt_lock);
	std::cv_status s = ((std::condition_variable*)this->condition_ident)->wait_for(lock, std::chrono::milliseconds(ms));
	lock.release();
	if (s == std::cv_status::timeout) {
		return false;
	}
	else { /* (s == std::cv_status::no_timeout) */
		return true;
	}
#endif
}

void VR_Network::Thread::Condition::leave_silent()
{
#ifdef WIN32
	LeaveCriticalSection((CRITICAL_SECTION*)this->mutex_ident);
#else
	((std::mutex*)this->mutex_ident)->unlock();
#endif
}

void VR_Network::Thread::Condition::leave_signal(bool wake_all)
{
#ifdef WIN32
	LeaveCriticalSection((CRITICAL_SECTION*)this->mutex_ident);
	if (wake_all) {
		WakeAllConditionVariable((CONDITION_VARIABLE*)this->condition_ident);
	}
	else {
		WakeConditionVariable((CONDITION_VARIABLE*)this->condition_ident);
	}
#else
	std::unique_lock<std::mutex> lock(*(std::mutex*)mutex_ident, std::adopt_lock);
	if (wake_all) {
		((std::condition_variable*)this->condition_ident)->notify_all();
	}
	else {
		((std::condition_variable*)this->condition_ident)->notify_one();
	}
#endif
}

/***************************************************************************************************
 *											 vr_api
 ***************************************************************************************************/
/* Start remote device stream. */
int vr_api_init_remote(int timeout_sec)
{
	VR_Network::start();

#ifdef WIN32
	int timeout = timeout_sec * CLOCKS_PER_SEC;
	clock_t start = clock();
	while (!VR_Network::initialized && (clock() - start < timeout)) {
		Sleep(1000);
	}
#else
	int timeout = timeout_sec;
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	time_t start = ts.tv_sec;
	while (!VR_Network::initialized && (ts.tv_sec - start < timeout)) {
		usleep(1000000);
		clock_gettime(CLOCK_REALTIME, &ts);
	}
#endif

	if (VR_Network::initialized) {
		return 0;
	}

	VR_Network::stop();
	return -1;
}

/* Transfer remote VR params to VR module. */
int vr_api_get_params_remote()
{
	VR& vr = *vr_get_obj();
	VR_Network::NetworkData& data = *(VR_Network::NetworkData*)VR_Network::recv_buf;

	memcpy(vr.fx, data.fx, sizeof(float) * 2);
	memcpy(vr.fy, data.fy, sizeof(float) * 2);
	memcpy(vr.cx, data.cx, sizeof(float) * 2);
	memcpy(vr.cy, data.cy, sizeof(float) * 2);
	vr.tex_width = data.tex_width;
	vr.tex_height = data.tex_height;

	return 0;
}

/* Transfer remote tracking transforms to VR module. */
int vr_api_get_transforms_remote()
{
	VR& vr = *vr_get_obj();
	VR_Network::NetworkData& data = *(VR_Network::NetworkData*)VR_Network::recv_buf;

	memcpy(vr.t_eye[VR_SPACE_REAL], data.t_eye, sizeof(float) * 4 * 4 * 2);
	memcpy(vr.t_hmd[VR_SPACE_REAL], data.t_hmd, sizeof(float) * 4 * 4);
	memcpy(vr.t_controller[VR_SPACE_REAL], data.t_controller, sizeof(float) * 4 * 4 * VR_MAX_CONTROLLERS);

	return 0;
}

/* Transfer remote controller states to VR module. */
int vr_api_get_controller_states_remote()
{
	VR& vr = *vr_get_obj();
	VR_Network::NetworkData& data = *(VR_Network::NetworkData*)VR_Network::recv_buf;

	for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
		memcpy(vr.controller[i], &data.controller[i], sizeof(VR_Controller));
	}

	return 0;
}

/* Stop remote device stream. */
int vr_api_uninit_remote(int timeout_sec)
{
	VR_Network::stop();

#ifdef WIN32
	int timeout = timeout_sec * CLOCKS_PER_SEC;
	clock_t start = clock();
	while (VR_Network::thread && (clock() - start) < timeout) {
		Sleep(1000);
	}
#else
	int timeout = timeout_sec;
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	time_t start = ts.tv_sec;
	while (VR_Network::thread && (ts.tv_sec - start < timeout)) {
		usleep(1000000);
		clock_gettime(CLOCK_REALTIME, &ts);
	}
#endif

	if (!VR_Network::thread) {
		return 0;
	}

	return -1;
}
