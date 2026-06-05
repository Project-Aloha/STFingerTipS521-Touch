// Copyright (c) Microsoft Corporation. All Rights Reserved. 
// Copyright (c) Bingxing Wang. All Rights Reserved. 

#pragma once

#include "controller.h"
#include <report.h>

#define DEFINE_GUID2(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
		EXTERN_C const GUID DECLSPEC_SELECTANY name \
				= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

// AC/DC power source
// ------------------
//

// Specifies the power source for the system.  consumers may register for
// notification when the power source changes and will be notified with
// one of 3 values:
// 0 - Indicates the system is being powered by an AC power source.
// 1 - Indicates the system is being powered by a DC power source.
// 2 - Indicates the system is being powered by a short-term DC power
//	 source.  For example, this would be the case if the system is
//	 being powed by a short-term battery supply in a backing UPS
//	 system.  When this value is recieved, the consumer should make
//	 preparations for either a system hibernate or system shutdown.
//
// { 5D3E9A59-E9D5-4B00-A6BD-FF34FF516548 }
DEFINE_GUID2(GUID_ACDC_POWER_SOURCE, 0x5D3E9A59, 0xE9D5, 0x4B00, 0xA6, 0xBD, 0xFF, 0x34, 0xFF, 0x51, 0x65, 0x48);

//
// Specifies a change in the current monitor's display state.
//
// {6fe69556-704a-47a0-8f24-c28d936fda47}
//
DEFINE_GUID2(GUID_CONSOLE_DISPLAY_STATE, 0x6fe69556, 0x704a, 0x47a0, 0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47);

#define TOUCH_DELAY_TO_COMMUNICATE 200000
#define TOUCH_POWER_RAIL_STABLE_TIME 2000
#define TOUCH_CONTROLLER_LOCK_TIMEOUT_MS 5000
#define TOUCH_SPB_LOCK_TIMEOUT_MS 5000
#define TOUCH_SPB_TRANSFER_TIMEOUT_MS 5000
#define TOUCH_GPIO_TRANSFER_TIMEOUT_MS 5000
#define TOUCH_FTS_COMMAND_ECHO_TIMEOUT_MS 2500
#define TOUCH_FTS_COMMAND_ECHO_POLL_INTERVAL_MS 50
#define TOUCH_CONTROLLER_READY_TIMEOUT_MS 3000
#define TOUCH_CONTROLLER_READY_POLL_INTERVAL_MS 20
#define TOUCH_CONTROLLER_RESET_ATTEMPTS 3
#define TOUCH_HID_IDLE_D3_WAKE_SUPPORTED 0
#define TOUCH_DISPLAY_STATE_UNKNOWN ((ULONG)-1)
#define TOUCH_DISPLAY_STATE_QUEUE_DEPTH 16
#define TOUCH_REL_TIMEOUT_MS(ms) (-(10 * 1000 * (LONGLONG)(ms)))

typedef enum _TOUCH_RUNTIME_STATE
{
	TouchRuntimeUninitialized = 0,
	TouchRuntimePrepared,
	TouchRuntimeStarted,
	TouchRuntimeD0Active,
	TouchRuntimeDisplayOff,
	TouchRuntimeResetting,
	TouchRuntimeD3,
	TouchRuntimeFailed
} TOUCH_RUNTIME_STATE;

typedef enum _TOUCH_INTERRUPT_STATE
{
	TouchInterruptNotCreated = 0,
	TouchInterruptCreated,
	TouchInterruptWdfEnabled,
	TouchInterruptWdfDisabled
} TOUCH_INTERRUPT_STATE;

typedef enum _TOUCH_RESET_STATE
{
	TouchResetIdle = 0,
	TouchResetAssertLow,
	TouchResetReleaseHigh,
	TouchResetWaitControllerReady,
	TouchResetComplete,
	TouchResetFailed
} TOUCH_RESET_STATE;

typedef enum _TOUCH_IDLE_STATE
{
	TouchIdleNone = 0,
	TouchIdleCallbackQueued,
	TouchIdleForwarding,
	TouchIdleCompletionRequested,
	TouchIdleParked,
	TouchIdleCompleting
} TOUCH_IDLE_STATE;

typedef struct _TOUCH_DISPLAY_STATE_EVENT
{
	ULONG DisplayState;
	ULONG PreviousDisplayState;
	ULONG Sequence;
} TOUCH_DISPLAY_STATE_EVENT;

typedef struct _TOUCH_POWER_CONTEXT
{
	WDFIOTARGET TouchPowerIOTarget;
	BOOLEAN TouchPowerOpen;
	PVOID TouchPowerNotify;
} TOUCH_POWER_CONTEXT;

//
// Device context
//

typedef struct _DEVICE_EXTENSION
{
	//
	// HID Touch input mode (touch vs. mouse)
	// 
	UCHAR InputMode;

	//
	// Device related
	//
	WDFDEVICE FxDevice;
	WDFQUEUE DefaultQueue;

	//
	// Interrupt servicing
	//
	WDFINTERRUPT InterruptObject;
	BOOLEAN ServiceInterruptsAfterD0Entry;
	TOUCH_INTERRUPT_STATE InterruptState;
	TOUCH_RUNTIME_STATE RuntimeState;
	TOUCH_RESET_STATE ResetState;
	ULONG LastDisplayState;
	
	//
	// Spb (I2C) related members used for the lifetime of the device
	//
	SPB_CONTEXT I2CContext;

	//
	// Reset GPIO line in case it exists used for power up sequence of the controller
	//
	LARGE_INTEGER ResetGpioId;
	WDFIOTARGET ResetGpio;
	BOOLEAN HasResetGpio;

	//
	// Test related
	//
	WDFQUEUE TestQueue;
	volatile LONG TestSessionRefCnt;
	BOOLEAN DiagnosticMode;

	//
	// Power related
	//
	WDFQUEUE IdleQueue;
	WDFWAITLOCK IdleLock;
	WDFREQUEST IdleRequest;
	TOUCH_IDLE_STATE IdleState;
	NTSTATUS IdleCompletionStatus;
	ULONG IdleSequence;
	WDFWAITLOCK DisplayStateLock;
	WDFWORKITEM DisplayStateWorkItem;
	TOUCH_DISPLAY_STATE_EVENT DisplayStateQueue[TOUCH_DISPLAY_STATE_QUEUE_DEPTH];
	ULONG DisplayStateQueueHead;
	ULONG DisplayStateQueueCount;
	ULONG DisplayStateSequence;
	BOOLEAN DisplayStateWorkQueued;
	BOOLEAN DisplayStateHardwareReady;
	BOOLEAN DisplayStateAcceptingWork;

	//
	// Touch related members used for the lifetime of the device
	//
	VOID *TouchContext;

	//
	// Settings
	//
	TOUCH_SCREEN_SETTINGS TouchSettings;

	//
	// Report
	//
	REPORT_CONTEXT ReportContext;
	BOOLEAN ReportDone;

	//
	// PTP New
	//
	BOOLEAN PtpInputOn;

	//
	// PoFx
	//
	PVOID PoFxPowerSettingCallbackHandle1;
	PVOID PoFxPowerSettingCallbackHandle2;

	//
	// Touch Power
	//
	TOUCH_POWER_CONTEXT TouchPowerContext;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS
TchRestoreConsoleDisplayState(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
);

NTSTATUS
TchCreateDisplayStateWorker(
	IN WDFDEVICE FxDevice
);

VOID
TchFlushDisplayStateWorker(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
);

NTSTATUS
TchStopDisplayStateHardwareWork(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
);

NTSTATUS
TchDisableDisplayStateHardwareWork(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
);

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, GetDeviceContext)
