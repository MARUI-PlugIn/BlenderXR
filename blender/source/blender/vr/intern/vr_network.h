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

/** \file blender/vr/intern/vr_network.h
*   \ingroup vr
*/

#ifndef __VR_NETWORK_H__
#define __VR_NETWORK_H__

#include "vr_types.h"
#include "vr_main.h"

#include <string>
#include <vector>
#include <atomic>

/* Size (bytes) of the VR data to send / receive. */
#define VR_NETWORK_RECV_BUF_SIZE	sizeof(VR_Network::NetworkData)
#define VR_NETWORK_SEND_BUF_SIZE	614400 // 320*240*4*2 //1382400 // 480*360*4*2 //2457600 //640*480*4*2 //9830400 // 1280*960*4*2
#define VR_NETWORK_SEND_BUF_SIZE_HALF 307200 // 320*240*4 //691200 // 480*360*4 //1228800 //640*480*4 //4915200 // 1280*960*4 

/* Whether to enable image streaming. */
#define VR_NETWORK_IMAGE_STREAMING 1

/* VR network module for remote streaming. */
class VR_Network
{
protected:
	/* Thread object. */
	class Thread
	{
		Thread(void *ident);		/* Constructor (hidden, use Thread::create() instead). */
		~Thread();					/* Destructor (hidden, use Thread::destroy() instead). */
		void *thread_ident;			/* Arbitrary pointer for thread identifier (OS dependent). */
	public:
		static Thread *create(void_func_ptr f);					/* Create a new thread. */
		static Thread *create(voidptr_func_ptr f, void *param);	/* Create a new thread. */
		static bool    destroy(Thread *thread);					/* Terminate and delete thread. */
		static void sleep(uint ms);	/* Suspend calling thread for a given time. */
		static bool delayed_call(uint ms, voidptr_func_ptr f, void *param);	/* Call a function after a given delay. */
    const void *id();       /* Get thread ID. */

		/* Runlevel utility enum.
		 * Runlevels are separated in two categories (which are themselves Runlevels)
		 * that can be used with an bitwise AND (&) operation to decide whether the
		 * runlevel is part of this category.
		 * The two main categories are "RUNLEVEL_ALIVE" and "RUNLEVEL_DEAD",
		 * describing whether the thread is active or not.
		 * The subtcategories can be used to furtherly describe the current or
		 * advised Runlevel for the thread. */
		typedef enum Runlevel {
			/* ALIVE Runlevels ```````````````````````````````````````````````````````````````` */
			RUNLEVEL_ALIVE			= 0x0001	/* The thread is alive, running or suspended. */
			,
			RUNLEVEL_RUNNING		= 0x0011    /* The thread is alive and running (but possibly sleeping). */
			,
			RUNLEVEL_SLEEPING		= 0x0111    /* The thread is alive, but currently sleeping or waiting for resource. */
			,
			RUNLEVEL_TERMINATING	= 0x0211    /* The thread is alive, but ordered to terminate. */
			,
			RUNLEVEL_SUSPENDED		= 0x0021    /* The thread is alive but suspended. */
			,
			RUNLEVEL_CANTTERMINATE	= 0x0121    /* The thread is alive but suspended. It's not needed anymore but can't be terminated for some reason. */
			,
			RUNLEVEL_CANTRECOVER	= 0x0221    /* The thread is alive but suspended. It's still needed but can't return to operation for some reason. */
			, /* DEAD Runlevels ````````````````````````````````````````````````````````````````` */
			RUNLEVEL_DEAD			= 0x0002    /* The thread is dead, either not started yet or terminated. */
			,
			RUNLEVEL_UNSTARTED		= 0x0012    /* The thread is dead, because it was not started yet. */
			,
			RUNLEVEL_UNINITIALIZED	= 0x0112    /* The thread is dead. It cant's start because it was not initialized yet. */
			,
			RUNLEVEL_READY			= 0x0212    /* The thread is dead, not started yet, but it's ready to start. */
			,
			RUNLEVEL_TERMINATED		= 0x0022    /* The thread is dead. It ran but it was terminated or terminated itself. */
			,
			RUNLEVEL_ENDED			= 0x0122    /* The thread is dead. It terminated itself for some reason (maybe it finished its task). */
			,
			RUNLEVEL_KILLED			= 0x0222    /* The thread is dead. It was killed by an external agent. */
		} Runlevel;	/* Categorized Runlevels, can be used with bitwise operators. */

		/* Thread Condition Variable object. */
		class Condition {
			void *mutex_ident;		/* Generic pointer to OS-specific mutex/critical section identifier. */
			void *condition_ident;	/* Generic pointer to OS-specific conditional variable identifier. */
		public:
			Condition();			/* Constructor. */
			~Condition();			/* Destructor. */
			void enter();			 /* Enter associated critical section. */
			void wait();			/* Wait for signal (within the critical section, temporarily leaving it). */
			bool wait(uint ms);		/* Wait for signal (within the critical section, temporarily leaving it). */
			void leave_silent();	/* Leave the critical section without waking waiting threads. */
			void leave_signal(bool wake_all = false);	/* Leave the critical section, signaling waiting threads. */
		};
	};

public:
	/* Network statuses. */
	typedef enum NetworkStatus {
		NETWORKSTATUS_INACTIVE = 0	/* The networking thread is not running. */
		,
		NETWORKSTATUS_NOTCONNECTED = 1	/* The streaming service is inactive. */
		,
		NETWORKSTATUS_STARTINGNETWORK = 2	/* The networking is being started. */
		,
		NETWORKSTATUS_WAITINGFORCLIENT = 3	/* Networking is running but the client machine has not connected yet. */
		,
		NETWORKSTATUS_CONNECTED = 4	/* The app is connected and data transfer is taking place. */
		,
		NETWORKSTATUS_DISCONNECT = 5	/* The client machine disconnected. */
		,
		NETWORKSTATUS_NETWORKSHUTDOWN = 6	/* The networking is being shut down. */
	} NetworkStatus;

  /* Simple struct to hold information about network adapters. */
  struct NetworkAdapter {
    std::string ip_address;	/* IPv4 address assigned to this adapter. */
    std::string name;	/* Adapter name. */
  };
  static std::vector<NetworkAdapter> network_adapters;	/* List of network adapters. */
  static bool update_network_adapters();	/* Update the current list of network adapters. */

 /* Network data to receive. */
  typedef struct NetworkData {
    VR_Device_Type device_type; /* Type of VR device used. */
    int tracking;		/* Whether the VR tracking state is currently active/valid. */
    float fx[VR_SIDES]; /* Horizontal focal length, in "image-width" - units(1 = image width). */
    float fy[VR_SIDES]; /* Vertical focal length, in "image-height" - units(1 = image height). */
    float cx[VR_SIDES]; /* Horizontal principal point, in "image-width" - units(0.5 = image center). */
    float cy[VR_SIDES]; /* Vertical principal point, in "image-height" - units(0.5 = image center). */
    int tex_width;		/* Default eye texture width. */
    int tex_height;		/* Default eye texture height. */
    float aperture_u;	/* The aperture of the texture(0~u) that contains the rendering. */
    float aperture_v;	/* The aperture of the texture(0~v) that contains the rendering. */
    float t_hmd[4][4];	/* Last tracked position of the HMD. */
    float t_eye[VR_SIDES][4][4];	/* Last tracked position of the eyes. */
    VR_Controller controller[VR_MAX_CONTROLLERS];		/* Controllers associated with the HMD device. */
    float t_controller[VR_MAX_CONTROLLERS][4][4];	/* Last tracked positions of the controllers. */
  } NetworkData;

  /* Image data to send. */
  typedef struct ImageData {
    uint w;	/* Image width in pixels. */
    uint h;	/* Image height in pixels. */
    uint d; /* Image depth. */
    uchar *buf;	/* Image buffer to receive the image pixel data to be sent. */
    uint compressed_size;	/* Size of the encoded image buffer in bytes. */
    uint quality; /* PNG compression quality. */
  } ImageData;
  static ImageData image_data[VR_SIDES];

  static bool set_image_size(uint width, uint height, uint depth);	/* Set the desired image dimensions. */
  static bool resample_pixels(const uchar *pixels, uint w_old, uint h_old,
    uchar *pixels_new, uint w_new, uint h_new, uint depth,
    const uint *depth_buffer = 0);	/* Image resampler helper function.*/

  static char recv_buf[VR_NETWORK_RECV_BUF_SIZE];	/* Buffer for receiving VR data. */
  static char send_buf[VR_NETWORK_SEND_BUF_SIZE];	/* Buffer for sending VR data. */

  static char control_sequence[4];  /* Control sequence for sending / receiving network data. */
  static bool initialized;  /* Whether the VR params have been initialized / received from client device. */
  static std::atomic<bool> data_new;	/* Whether the data is new / was already sent. */
  static std::atomic<bool> img_processed; /* Whether the image data has been processed. */

  static NetworkStatus network_status;	/* Current status of networking. */

  static void *thread;	/* Networking thread handle. */
  static Thread::Runlevel	runlvl;	/* Networking thread runlevel. */
  static Thread::Condition condition;	/* Condition variable for accessing the VR data buffer. */
  static void *img_thread;	/* Image processing thread handle. */
  static Thread::Runlevel	img_runlvl;	/* Image thread runlevel. */
  static Thread::Condition img_condition;	/* Condition variable for accessing the image data. */

#ifdef WIN32
  static bool receive_data(unsigned long long& socket); /* Receive data from client. */
  static bool send_data(unsigned long long& socket);  /* Send data to client. */
  static uint __stdcall thread_func(void *data);	/* Thread function for the networking thread (to external machine via WiFi). */
  static uint __stdcall img_thread_func(void *data);	/* Thread function for the image processing thread. */
#else
  static bool receive_data(int& socket); /* Receive data from client. */
  static bool send_data(int& socket); /* Send data to client. */
  static void thread_func();	/* Thread function for the networking thread (to external machine via WiFi). */
  static void img_thread_func();	/* Thread function for the image processing thread. */
#endif

  static bool start();	/* Start networking. */
  static bool stop();	/* Stop networking. */
};

#endif /* __VR_NETWORK_H__ */
