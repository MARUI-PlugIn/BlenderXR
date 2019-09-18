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
Use the CMakeLists.txt provided in the main folder. Tested with gcc 7.3+.

## COPYRIGHT/LICENSE:
The BlenderXR source code is copyright (c) 2018 MARUI-PlugIn (inc.)
and is made available under the GNU GENERAL PUBLIC LICENSE Version 3 (see: LICENSE file).
Blender XR makes use of 3rd party libraries not covered by the GPL. Therefore we are granting an additional permission under section 7:
(see: https://www.gnu.org/licenses/gpl-faq.html#GPLIncompatibleLibs )
> Copyright (C) 2018 MARUI-PlugIn (inc.)
>
> This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.
>
> This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
>
> You should have received a copy of the GNU General Public License along with this program; if not, see <https://www.gnu.org/licenses>.
>
> Additional permission under GNU GPL version 3 section 7
>
> If you modify this Program, or any covered work, by linking or combining it with BlenderXR (or a modified version of that library), containing parts covered by the terms of FOVE SDK license, The OpenGL Extension Wrangler Library license, the BSD 3-Clause "New" or "Revised" License, or the Oculus Software Development Kit License Agreement, the licensors of this Program grant you additional permission to convey the resulting work. Corresponding Source for a non-source form of such a combination shall include the source code for the parts of BlenderXR used as well as that of the covered work.



### Included third-party libraries
This repository includes the following third-party libraries in un-altered form.
The respective copy right holders rights and licensing terms apply.

#### FoveSDK (v0.15.0)
/inc/FoveSDK_0.15.0/*
/lib/FoveSDK_0.15.0/*
/bin/Windows/Fove/FoveClient.dll
Copyright (c) 2016 FOVE, Inc.
https://www.getfove.com/developers/

#### GLEW (v1.13.0)
/inc/glew_1.13.0/*
/lib/glew_1.13.0/*
Copyright (C) 2002-2007, Milan Ikits <milan ikits[]ieee org>
Copyright (C) 2002-2007, Marcelo E. Magallon <mmagallo[]debian org>
Copyright (C) 2002, Lev Povalahev
https://github.com/nigels-com/glew

#### Oculus SDK / LibOVR (v1.26.0)
/inc/OculusSDK_1.26.0/*
/lib/OculusSDK_1.26.0/*
Copyright (c) Facebook Technologies, LLC and its affiliates.
https://developer.oculus.com/downloads/package/oculus-sdk-for-windows/
https://developer.oculus.com/licenses/sdk-3.4.1/

#### openvr_1.0.13
/inc/openvr_1.0.13/*
/lib/openvr_1.0.13/*
/bin/Windows/SteamVR/openvr_api.dll
/bin/Linux/SteamVR/libopenvr_api.so
Copyright (c) 2015, Valve Corporation
https://github.com/ValveSoftware/openvr/blob/master/LICENSE


### Included Blender source code
This repository also contains a derived version of Blender which can utilize the BlenderXR library.
The original Blender source code is copyright (c) 2010 Blender Foundation.
https://www.blender.org
The derived Blender version in this repository is covered under the GPL.
