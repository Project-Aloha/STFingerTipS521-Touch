// Copyright (c) Microsoft Corporation. All Rights Reserved. 
// Copyright (c) Bingxing Wang. All Rights Reserved. 

#pragma once

#include <wdm.h>
#include <wdf.h>
#include <hidport.h>
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include "trace.h"
#include "hid.h"
#include "spb.h"

//
// Memory tags
//
#define TOUCH_POOL_TAG					(ULONG)'cuoT'
#define TOUCH_POOL_TAG_F12				(ULONG)'21oT'
#define TOUCH_POWER_POOL_TAG			(ULONG)'PuoT'

//
// Constants
//
#define MODE_MULTI_TOUCH			0x02
#define MAX_TOUCH_COORD				0x0FFF
#define FINGER_STATUS				0x01 // finger down

//
// Structures
//
typedef struct _TOUCH_SCREEN_SETTINGS
{
	UINT32 DeviceId;
	UINT32 UseControllerSleep;
	UINT32 UseNoSleepBit;
	UINT32 ImprovedTouchSupported;
	UINT32 WakeupGestureSupported;
	UINT32 ChargerDetectionSupported;
	UINT32 ActivePenSupported;
	UINT32 ExtClockControlSupported;
	UINT32 ForceDriverSupported;
	UINT32 DoubleTapMaxTapTime10ms;
	UINT32 DoubleTapMaxTapDistance100um;
	UINT32 DoubleTapDeadZoneWidth100um;
	UINT32 DoubleTapDeadZoneHeight100um;
	UINT32 ControllerType;
	UINT32 VendorCount;
	UINT32 ResetControllerInWakeUp;
} TOUCH_SCREEN_SETTINGS, * PTOUCH_SCREEN_SETTINGS;

NTSTATUS 
TchAllocateContext(
	OUT VOID **ControllerContext,
	IN WDFDEVICE FxDevice
	);

NTSTATUS 
TchFreeContext(
	IN VOID *ControllerContext
	);

NTSTATUS
TchStartDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext
);

NTSTATUS 
TchStopDevice(
	IN VOID *ControllerContext,
	IN SPB_CONTEXT *SpbContext
	);

NTSTATUS
TchStandbyDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN VOID* ReportContext
	);

NTSTATUS 
TchWakeDevice(
	IN VOID *ControllerContext,
	IN SPB_CONTEXT *SpbContext
	);

NTSTATUS
RtlReadRegistryValue(
	PCWSTR registry_path, 
	PCWSTR value_name, 
	ULONG type, 
	PVOID data, 
	ULONG length
	);

NTSTATUS
TchRegistryGetControllerSettings(
	IN VOID *ControllerContext,
	IN WDFDEVICE FxDevice
	);

VOID
TchGetTouchSettings(
	IN PTOUCH_SCREEN_SETTINGS TouchSettings
);

NTSTATUS
TchPowerSettingCallback(
	_In_ LPCGUID SettingGuid,
	_In_ PVOID Value,
	_In_ ULONG ValueLength,
	_Inout_opt_ PVOID Context
);