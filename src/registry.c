/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Copyright (c) Bingxing Wang. All Rights Reserved.
	Copyright (c) LumiaWoA authors. All Rights Reserved.

	Module Name:

		registry.c

	Abstract:

		This module retrieves platform-specific controller
		configuration from the registry, or assigns default
		values if no registry configuration is present.

	Environment:

		Kernel mode

	Revision History:

--*/

#include <fts521\fts521internal.h>
#include <registry.tmh>
#include <internal.h>

#define TOUCH_REG_KEY					 L"\\Registry\\Machine\\SYSTEM\\TOUCH"
#define TOUCH_SCREEN_SETTINGS_SUB_KEY	 L"Settings"
#define TOUCH_SCREEN_SETTINGS_00_SUB_KEY L"Settings\\00"
#define TOUCH_SCREEN_SETTINGS_01_SUB_KEY L"Settings\\01"
#define TOUCH_SCREEN_SETTINGS_02_SUB_KEY L"Settings\\02"
#define TOUCH_SCREEN_SETTINGS_03_SUB_KEY L"Settings\\03"
#define TOUCH_SCREEN_SETTINGS_FF_SUB_KEY L"Settings\\FF"

//
// Default FTS521 configuration values can be changed here. Please refer to the
// FTS521 specification for a full description of the fields and value meanings
//

static FTS521_CONFIGURATION gDefaultConfiguration =
{
	//
	// FTS521 F01 - Device control settings
	//
	{
		0,												// Sleep Mode (normal)
		1,												// No Sleep (do sleep)
		0,												// Report Rate (standard)
		1,												// Configured
		0xff,											// Interrupt Enable
		FTS521_MILLISECONDS_TO_TENTH_MILLISECONDS(20),	// Doze Interval
		10,												// Doze Threshold
		FTS521_SECONDS_TO_HALF_SECONDS(2)				// Doze Holdoff
	},

	//
	// FTS521 F11 - 2D Touchpad sensor settings
	//
	{
		1,											// Reporting mode (throttle)
		1,											// Abs position filter
		0,											// Rel position filter
		0,											// Rel ballistics
		0,											// Dribble
		0xb,										// PalmDetectThreshold
		3,											// MotionSensitivity
		0,											// ManTrackEn
		0,											// ManTrackedFinger
		0,											// DeltaXPosThreshold
		0,											// DeltaYPosThreshold
		0,											// Velocity
		0,											// Acceleration
		TOUCH_DEVICE_RESOLUTION_X,					// Sensor Max X Position
		TOUCH_DEVICE_RESOLUTION_Y,					// Sensor Max Y Position
		0x1e,										// ZTouchThreshold
		0x05,										// ZHysteresis
		0x28,										// SmallZThreshold
		0x28f5,										// SmallZScaleFactor
		0x051e,										// LargeZScaleFactor
		0x1,										// AlgorithmSelection
		0x30,										// WxScaleFactor
		0x0,										// WxOffset
		0x30,										// WyScaleFactor
		0x0,										// WyOffset
		0x4800,										// XPitch
		0x4800,										// YPitch
		0xea4f,										// FingerWidthX
		0xdf6c,										// FingerWidthY
		0,											// ReportMeasuredSize
		0x70,										// SegmentationSensitivity
		0x0,										// XClipLo
		0x0,										// XClipHi
		0x0,										// YClipLo
		0x0,										// YClipHi
		0x0a,										// MinFingerSeparation
		0x04										// MaxFingerMovement
	},

	//
	// Internal driver settings
	//
	{
		0x0,										// Controller stays powered in D3
	},
};

static TOUCH_SCREEN_SETTINGS gDefaultTouchSettings =
{
	0x1,										// DeviceId
	0x0,										// UseControllerSleep
	0x1,										// UseNoSleepBit
	0x0,										// ImprovedTouchSupported
	0x0,										// WakeupGestureSupported
	0x0,										// ChargerDetectionSupported
	0x0,										// ActivePenSupported
	0x0,										// ExtClockControlSupported
	0x0,										// ForceDriverSupported
	0x0,										// DoubleTapMaxTapTime10ms
	0x3C,										// DoubleTapMaxTapDistance100um
	0x32,										// DoubleTapDeadZoneWidth100um
	0x32,										// DoubleTapDeadZoneHeight100um
	0x32,										// ControllerType
	0x1,										// VendorCount
	0x0,										// ResetControllerInWakeUp
};

RTL_QUERY_REGISTRY_TABLE gRegistryTable[] =
{
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"DeviceId",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, DeviceId)),
		REG_DWORD,
		&gDefaultTouchSettings.DeviceId,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"UseControllerSleep",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, UseControllerSleep)),
		REG_DWORD,
		&gDefaultTouchSettings.UseControllerSleep,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"UseNoSleepBit",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, UseNoSleepBit)),
		REG_DWORD,
		&gDefaultTouchSettings.UseNoSleepBit,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"ImprovedTouchSupported",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, ImprovedTouchSupported)),
		REG_DWORD,
		&gDefaultTouchSettings.ImprovedTouchSupported,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"WakeupGestureSupported",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, WakeupGestureSupported)),
		REG_DWORD,
		&gDefaultTouchSettings.WakeupGestureSupported,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"ChargerDetectionSupported",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, ChargerDetectionSupported)),
		REG_DWORD,
		&gDefaultTouchSettings.ChargerDetectionSupported,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"ActivePenSupported",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, ActivePenSupported)),
		REG_DWORD,
		&gDefaultTouchSettings.ActivePenSupported,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"ExtClockControlSupported",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, ExtClockControlSupported)),
		REG_DWORD,
		&gDefaultTouchSettings.ExtClockControlSupported,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"ForceDriverSupported",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, ForceDriverSupported)),
		REG_DWORD,
		&gDefaultTouchSettings.ForceDriverSupported,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"DoubleTapMaxTapTime10ms",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, DoubleTapMaxTapTime10ms)),
		REG_DWORD,
		&gDefaultTouchSettings.DoubleTapMaxTapTime10ms,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"DoubleTapMaxTapDistance100um",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, DoubleTapMaxTapDistance100um)),
		REG_DWORD,
		&gDefaultTouchSettings.DoubleTapMaxTapDistance100um,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"DoubleTapDeadZoneWidth100um",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, DoubleTapDeadZoneWidth100um)),
		REG_DWORD,
		&gDefaultTouchSettings.DoubleTapDeadZoneWidth100um,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"DoubleTapDeadZoneHeight100um",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, DoubleTapDeadZoneHeight100um)),
		REG_DWORD,
		&gDefaultTouchSettings.DoubleTapDeadZoneHeight100um,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"ControllerType",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, ControllerType)),
		REG_DWORD,
		&gDefaultTouchSettings.ControllerType,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"VendorCount",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, VendorCount)),
		REG_DWORD,
		&gDefaultTouchSettings.VendorCount,
		sizeof(UINT32)
	},
	{
		NULL, RTL_QUERY_REGISTRY_DIRECT,
		L"ResetControllerInWakeUp",
		(PVOID)(FIELD_OFFSET(TOUCH_SCREEN_SETTINGS, ResetControllerInWakeUp)),
		REG_DWORD,
		&gDefaultTouchSettings.ResetControllerInWakeUp,
		sizeof(UINT32)
	},
	//
	// List Terminator
	//
	{
		NULL, 0,
		NULL,
		0,
		REG_DWORD,
		NULL,
		0
	}
};
static const ULONG gcbRegistryTable = sizeof(gRegistryTable);
static const ULONG gcRegistryTable =
sizeof(gRegistryTable) / sizeof(gRegistryTable[0]);

NTSTATUS
RtlReadRegistryValue(
	PCWSTR registry_path,
	PCWSTR value_name,
	ULONG type,
	PVOID data,
	ULONG length
)
{
	UNICODE_STRING valname;
	UNICODE_STRING keyname;
	OBJECT_ATTRIBUTES attribs;
	PKEY_VALUE_PARTIAL_INFORMATION pinfo;
	HANDLE handle;
	NTSTATUS rc;
	ULONG len, reslen;

	RtlInitUnicodeString(&keyname, registry_path);
	RtlInitUnicodeString(&valname, value_name);

	InitializeObjectAttributes(
		&attribs, 
		&keyname, 
		OBJ_CASE_INSENSITIVE,
		NULL, 
		NULL
	);

	rc = ZwOpenKey(
		&handle, 
		KEY_QUERY_VALUE, 
		&attribs
	);

	if (!NT_SUCCESS(rc))
	{
		return 0;
	}

	len = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + length;

	pinfo = ExAllocatePoolWithTag(
		NonPagedPool, 
		len, 
		TOUCH_POOL_TAG
	);

	if (pinfo == NULL)
	{
		goto exit;
	}

	rc = ZwQueryValueKey(
		handle, 
		&valname, 
		KeyValuePartialInformation,
		pinfo, 
		len, 
		&reslen
	);

	if ((NT_SUCCESS(rc) || rc == STATUS_BUFFER_OVERFLOW) && 
		reslen >= (sizeof(KEY_VALUE_PARTIAL_INFORMATION) - 1) &&
		(!type || pinfo->Type == type))
	{
		reslen = pinfo->DataLength;
		memcpy(data, pinfo->Data, min(length, reslen));
	}
	else
	{
		reslen = 0;
	}

	if (pinfo != NULL)
	{
		ExFreePoolWithTag(pinfo, TOUCH_POOL_TAG);
	}

exit:
	ZwClose(handle);
	return rc;
}

NTSTATUS
TchRegistryGetControllerSettings(
	IN VOID* ControllerContext,
	IN WDFDEVICE FxDevice
)
/*++

  Routine Description:

	This routine retrieves controller wide settings
	from the registry.

  Arguments:

	FxDevice - a handle to the framework device object
	Settings - A pointer to the chip settings structure

  Return Value:

	NTSTATUS indicating success or failure

--*/
{
	FTS521_CONTROLLER_CONTEXT* controller;
	NTSTATUS status;

	UNREFERENCED_PARAMETER(FxDevice);

	controller = (FTS521_CONTROLLER_CONTEXT*)ControllerContext;

	RtlCopyMemory(
		&controller->Config,
		&gDefaultConfiguration,
		sizeof(FTS521_CONFIGURATION));

	status = STATUS_SUCCESS;

	return status;
}

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz = siz, truncation occurred.
 * 
 * From: https://stackoverflow.com/questions/1855956/how-do-you-concatenate-two-wchar-t-together
 */
size_t wstrlcat(wchar_t* dst, const wchar_t* src, size_t siz)
{
	wchar_t* d = dst;
	const wchar_t* s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != L'\0') {
		d++;
	}

	dlen = d - dst;
	n = siz - dlen;

	if (n == 0) {
		return(dlen + wcslen(s));
	}

	while (*s != L'\0')
	{
		if (n != 1)
		{
			*d++ = *s;
			n--;
		}
		s++;
	}

	*d = '\0';
	return(dlen + (s - src));		/* count does not include NUL */
}

VOID
TchGetTouchSettings(
	IN PTOUCH_SCREEN_SETTINGS TouchSettings
)
{
	ULONG i;
	PRTL_QUERY_REGISTRY_TABLE regTable;
	WCHAR regKey[120] = { 0 };
	NTSTATUS status;

	regTable = NULL;

	wstrlcat(regKey, TOUCH_REG_KEY, sizeof(TOUCH_REG_KEY));
	regKey[sizeof(TOUCH_REG_KEY) / sizeof(WCHAR)] = L'\\';
	RtlCopyMemory((PCHAR)regKey + sizeof(TOUCH_REG_KEY) + sizeof(WCHAR), TOUCH_SCREEN_SETTINGS_SUB_KEY, sizeof(TOUCH_SCREEN_SETTINGS_SUB_KEY) - sizeof(WCHAR));
	regKey[(sizeof(TOUCH_REG_KEY) + sizeof(TOUCH_SCREEN_SETTINGS_SUB_KEY)) / sizeof(WCHAR)] = L'\0';

	//
	// Table passed to RtlQueryRegistryValues must be allocated 
	// from NonPagedPool
	//
	regTable = ExAllocatePoolWithTag(
		NonPagedPool,
		gcbRegistryTable,
		TOUCH_POOL_TAG);

	if (regTable == NULL)
	{
		return;
	}

	RtlCopyMemory(
		regTable,
		gRegistryTable,
		gcbRegistryTable);

	//
	// Update offset values with base pointer
	// 
	for (i = 0; i < gcRegistryTable - 1; i++)
	{
		regTable[i].EntryContext = (PVOID)(
			((SIZE_T)regTable[i].EntryContext) +
			((ULONG_PTR)TouchSettings));
	}

	//
	// Start with default values
	//
	RtlCopyMemory(
		TouchSettings,
		&gDefaultTouchSettings,
		sizeof(TOUCH_SCREEN_SETTINGS));

	//
	// Populate device context with registry overrides (or defaults)
	//
	status = RtlQueryRegistryValues(
		RTL_REGISTRY_ABSOLUTE,
		regKey,
		regTable,
		NULL,
		NULL);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_WARNING,
			TRACE_REGISTRY,
			"Error retrieving registry configuration - 0x%08lX",
			status);
	}

	if (regTable != NULL)
	{
		ExFreePoolWithTag(regTable, TOUCH_POOL_TAG);
	}
}