# BlenderXR
BlenderXR library to use Blender with common VR/AR hardware.
It supports Oculus Rift, HTC Vive, WindowsMR (via SteamVR), and Fove headsets.


## HOW TO USE:
Pre-built binaries are provided in the /bin/ folder.
Select the sub-folder of your respective VR device and place the files in your Blender folder (next to your blender.exe file).

Fove:        /bin/Windows/Fove/

Oculus Rift: /bin/Windows/Oculus/

HTC Vive / WindowsMR: /bin/Windows/SteamVR/
                      /bin/Linux/SteamVR/ (Vive only)

## HOW TO BUILD:
Windows:
Visual Studio projects to build BlenderXR are provided in the /vs/ folder.

Linux:
Use the CMakeLists.txt provided in the main folder. Tested with gcc 7.3+ and clang 9.1+.

## COPYRIGHT/LICENSE:
The BlenderXR source code is copyright (c) 2018 MARUI-PlugIn (inc.)
and is made available under the GNU GENERAL PUBLIC LICENSE Version 3 (see: LICENSE file).


This repository includes the following third-party libraries in un-altered form.
The respective copy right holders rights and licensing terms apply.

### FoveSDK (v0.15.0)
/inc/FoveSDK_0.15.0/*
/lib/FoveSDK_0.15.0/*
/bin/Windows/Fove/FoveClient.dll
Copyright (c) 2016 FOVE, Inc.
https://www.getfove.com/developers/

### GLEW (v1.13.0)
/inc/glew_1.13.0/*
/lib/glew_1.13.0/*
Copyright (C) 2002-2007, Milan Ikits <milan ikits[]ieee org>
Copyright (C) 2002-2007, Marcelo E. Magallon <mmagallo[]debian org>
Copyright (C) 2002, Lev Povalahev
https://github.com/nigels-com/glew

### Oculus SDK / LibOVR (v1.26.0)
/inc/OculusSDK_1.26.0/*
/lib/OculusSDK_1.26.0/*
Copyright (c) Facebook Technologies, LLC and its affiliates.
https://developer.oculus.com/downloads/package/oculus-sdk-for-windows/
https://developer.oculus.com/licenses/sdk-3.4.1/

### openvr_1.0.13
/inc/openvr_1.0.13/*
/lib/openvr_1.0.13/*
/bin/Windows/SteamVR/openvr_api.dll
/bin/Linux/SteamVR/libopenvr_api.so
Copyright (c) 2015, Valve Corporation
https://github.com/ValveSoftware/openvr/blob/master/LICENSE
