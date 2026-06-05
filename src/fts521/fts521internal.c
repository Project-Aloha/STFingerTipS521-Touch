/*++
	  Copyright (c) Microsoft Corporation. All Rights Reserved.
	  Sample code. Dealpoint ID #843729.

	  Module Name:

			fts521internal.c

	  Abstract:

			Contains STFingerTipSTouch initialization code

	  Environment:

			Kernel mode

	  Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <report.h>
#include <fts521\fts521events.h>
#include <fts521\fts521internal.h>
#include <fts521\fts521regs.h>
#include <internal.h>
#include <fts521internal.tmh>

static
NTSTATUS
Fts521ReadEvent(
	IN SPB_CONTEXT* SpbContext,
	OUT BYTE EventData[FIFO_EVENT_SIZE]
)
{
	BYTE readCommand[1] = { FIFO_CMD_READONE };

	RtlZeroMemory(EventData, FIFO_EVENT_SIZE);
	return FtsWriteReadU8UX(
		SpbContext,
		readCommand,
		sizeof(readCommand),
		EventData,
		FIFO_EVENT_SIZE);
}

static
NTSTATUS
Fts521WaitForCommandEcho(
	IN SPB_CONTEXT* SpbContext,
	IN const BYTE* Command,
	IN ULONG CommandLength
)
{
	BYTE eventData[FIFO_EVENT_SIZE];
	LARGE_INTEGER delay;
	ULONG elapsedMs;
	ULONG matchLength;
	NTSTATUS status;

	if (Command == NULL || CommandLength == 0)
	{
		return STATUS_INVALID_PARAMETER;
	}

	matchLength = CommandLength;
	if (matchLength > 4)
	{
		matchLength = 4;
	}

	elapsedMs = 0;
	while (elapsedMs < TOUCH_FTS_COMMAND_ECHO_TIMEOUT_MS)
	{
		status = Fts521ReadEvent(SpbContext, eventData);
		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_REPORTING,
				"Fts521WaitForCommandEcho - FIFO read failed - 0x%08lX",
				status);
			return status;
		}

		if (eventData[0] == EVT_ID_STATUS_UPDATE &&
			eventData[1] == EVT_TYPE_STATUS_ECHO &&
			RtlCompareMemory(Command, &eventData[2], matchLength) == matchLength)
		{
			Trace(
				TRACE_LEVEL_INFORMATION,
				TRACE_REPORTING,
				"Fts521WaitForCommandEcho - Echo received after %lu ms",
				elapsedMs);
			return STATUS_SUCCESS;
		}

		if (eventData[0] == EVT_ID_ERROR)
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_REPORTING,
				"Fts521WaitForCommandEcho - Controller error event type=0x%02X while waiting for command 0x%02X",
				eventData[1],
				Command[0]);
			return STATUS_IO_DEVICE_ERROR;
		}

		if (eventData[0] != EVT_ID_NOEVENT)
		{
			Trace(
				TRACE_LEVEL_INFORMATION,
				TRACE_REPORTING,
				"Fts521WaitForCommandEcho - Observed event 0x%02X type=0x%02X while waiting for command 0x%02X",
				eventData[0],
				eventData[1],
				Command[0]);
		}

		delay.QuadPart = TOUCH_REL_TIMEOUT_MS(TOUCH_FTS_COMMAND_ECHO_POLL_INTERVAL_MS);
		KeDelayExecutionThread(KernelMode, FALSE, &delay);
		elapsedMs += TOUCH_FTS_COMMAND_ECHO_POLL_INTERVAL_MS;
	}

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"Fts521WaitForCommandEcho - Echo timeout after %lu ms for command 0x%02X",
		elapsedMs,
		Command[0]);
	return STATUS_TIMEOUT;
}

static
NTSTATUS
Fts521WriteSystemCommand(
	IN SPB_CONTEXT* SpbContext,
	IN BYTE SystemCommand,
	IN const BYTE* Settings,
	IN ULONG SettingsLength
)
{
	BYTE command[8];
	NTSTATUS status;

	if (SettingsLength > sizeof(command) - 2 ||
		(SettingsLength != 0 && Settings == NULL))
	{
		return STATUS_INVALID_PARAMETER;
	}

	command[0] = FTS_CMD_SYSTEM;
	command[1] = SystemCommand;
	if (SettingsLength != 0)
	{
		RtlCopyMemory(&command[2], Settings, SettingsLength);
	}

	status = SpbWriteDataSynchronously(
		SpbContext,
		command[0],
		&command[1],
		SettingsLength + 1);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_REPORTING,
			"Fts521WriteSystemCommand - Write failed sys=0x%02X settingsLength=%lu - 0x%08lX",
			SystemCommand,
			SettingsLength,
			status);
		return status;
	}

	return Fts521WaitForCommandEcho(
		SpbContext,
		command,
		SettingsLength + 2);
}

NTSTATUS
Fts521ServiceInterrupts(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN PREPORT_CONTEXT ReportContext
)
{
	NTSTATUS status = STATUS_NO_DATA_DETECTED;
	FTS521_CONTROLLER_CONTEXT* controller;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ServiceInterrupts - Entry");

	controller = (FTS521_CONTROLLER_CONTEXT*)ControllerContext;

	//
	// Grab a waitlock to ensure the ISR executes serially and is 
	// protected against power state transitions
	//
	LONGLONG lockTimeout = TOUCH_REL_TIMEOUT_MS(TOUCH_CONTROLLER_LOCK_TIMEOUT_MS);
	status = WdfWaitLockAcquire(controller->ControllerLock, &lockTimeout);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Timeout acquiring controller lock for interrupt service - 0x%08lX",
			status);

		goto exit_without_lock;
	}

	status = TchServiceObjectInterrupts(ControllerContext, SpbContext, ReportContext);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Fts521ServiceInterrupts - Error servicing interrupt - 0x%08lX",
			status);

		goto exit;
	}

exit:

	WdfWaitLockRelease(controller->ControllerLock);

exit_without_lock:

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ServiceInterrupts - Exit");

	return status;
}

NTSTATUS
Fts521BuildFunctionsTable(
	  IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	  IN SPB_CONTEXT* SpbContext
)
{
	  UNREFERENCED_PARAMETER(SpbContext);
	  UNREFERENCED_PARAMETER(ControllerContext);

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521BuildFunctionsTable - Entry");

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521BuildFunctionsTable - Exit");

	  return STATUS_SUCCESS;
}

NTSTATUS
Fts521ChangePage(
	  IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	  IN SPB_CONTEXT* SpbContext,
	  IN int DesiredPage
)
{
	  UNREFERENCED_PARAMETER(SpbContext);
	  UNREFERENCED_PARAMETER(ControllerContext);
	  UNREFERENCED_PARAMETER(DesiredPage);

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ChangePage - Entry");

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ChangePage - Exit");

	  return STATUS_SUCCESS;
}

NTSTATUS
Fts521ConfigureFunctions(
	  IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	  IN SPB_CONTEXT* SpbContext
)
{
	NTSTATUS status;

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"Fts521ConfigureFunctions - Entry");

	LARGE_INTEGER delay;

	UNREFERENCED_PARAMETER(ControllerContext);

	// FTS521_LOCKDOWN:
	// * Address: 0xA4
	// * { 0xA4, 0x06, FTS_CMD_LOCKDOWN_ID };
	BYTE FTS521_LOCKDOWN[2] = { 0x06, FTS_CMD_LOCKDOWN_ID };
	status = SpbWriteDataSynchronously(SpbContext, 0xA4 ,FTS521_LOCKDOWN, sizeof(FTS521_LOCKDOWN));

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Fts521ConfigureFunctions - Error Writing Lockdown code into the IC - 0x%08lX",
			status);
		goto exit;
	}

	DWORD GestureEnabled = 0;
	if (NT_SUCCESS(RtlReadRegistryValue(
		(PCWSTR)L"\\Registry\\Machine\\SOFTWARE\\OEM\\XiaoMi\\Touch\\WakeupGesture",
		(PCWSTR)L"Enabled",
		REG_DWORD,
		&GestureEnabled,
		sizeof(DWORD))) && GestureEnabled == 1)
	{
		// FTS521_GESTURE: 
		// * Address: 0xA2
		// * { 0xA2, 0x03, 0x20, 0x00, 0x00, 0x01 };
		BYTE FTS521_GESTURE[5] = { 0x03, 0x20, 0x00, 0x00, 0x01 };
		status = SpbWriteDataSynchronously(SpbContext, 0xA2, FTS521_GESTURE, sizeof(FTS521_GESTURE));

		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_INTERRUPT,
				"Fts521ConfigureFunctions - Error Enabling Gesture Mode for IC - 0x%08lX",
				status);
			goto exit;
		}
	}

	//Active Scan OFF
	status = Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, SCAN_MODE_ACTIVE_SETTINGS_NONE);
	if (!NT_SUCCESS(status))
	{
		goto exit;
	}

	delay.QuadPart = RELATIVE(MILLISECONDS(50));
	KeDelayExecutionThread(KernelMode, TRUE, &delay);

	//Active Scan ON
	status = Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, SCAN_MODE_ACTIVE_SETTINGS_ALL);

exit:
	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"Fts521ConfigureFunctions - Exit - 0x%08lX",
		status);

	return status;
}


NTSTATUS
Fts521SetScanMode(
	SPB_CONTEXT* SpbContext,
	BYTE Mode,
	BYTE Settings)
{
	NTSTATUS status;
	BYTE scanModeData[2];
	ULONG scanModeLength;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_INTERRUPT,
		"Fts521SetScanMode - Entry");


	scanModeData[0] = Mode;
	scanModeData[1] = Settings;

	switch (Mode)
	{
	case SCAN_MODE_ACTIVE:
		scanModeLength = 2;
		break;
	case SCAN_MODE_LOW_POWER:
		scanModeLength = 1;
		break;
	default:
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Fts521SetScanMode - Unknown scan mode 0x%02X",
			Mode);
		return STATUS_INVALID_PARAMETER;
	}

	status = SpbWriteDataSynchronously(SpbContext, FTS_CMD_SCAN_MODE, scanModeData, scanModeLength);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Fts521SetScanMode - Setting scan mode Error - 0x%08lX",
			status);
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_INTERRUPT,
		"Fts521SetScanMode - Exit - 0x%08lX",
		status);

	return status;
}

NTSTATUS
Fts521SetReportingFlags(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR NewMode,
	OUT UCHAR* OldMode
)
{
	  UNREFERENCED_PARAMETER(SpbContext);
	  UNREFERENCED_PARAMETER(ControllerContext);
	  UNREFERENCED_PARAMETER(NewMode);
	  UNREFERENCED_PARAMETER(OldMode);

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521SetReportingFlags - Entry");

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521SetReportingFlags - Exit");


	  return STATUS_SUCCESS;
}

NTSTATUS
Fts521ChangeChargerConnectedState(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR ChargerConnectedState
)
{
	UNREFERENCED_PARAMETER(ControllerContext);
	UNREFERENCED_PARAMETER(SpbContext);
	UNREFERENCED_PARAMETER(ChargerConnectedState);

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"Fts521ChangeChargerConnectedState - Entry");

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"Fts521ChangeChargerConnectedState - Not implemented for this controller");

	return STATUS_NOT_SUPPORTED;
}

NTSTATUS
Fts521ChangeSleepState(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR SleepState
)
{
	  UNREFERENCED_PARAMETER(ControllerContext);
	  NTSTATUS status;

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ChangeSleepState - Entry");

	  switch (SleepState)
	  {
	  case FTS521_F01_DEVICE_CONTROL_SLEEP_MODE_OPERATING:
		  status = Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, SCAN_MODE_ACTIVE_SETTINGS_ALL);
		  break;
	  case FTS521_F01_DEVICE_CONTROL_SLEEP_MODE_SLEEPING:
		  status = Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, SCAN_MODE_ACTIVE_SETTINGS_NONE);
		  break;
	  default:
		  Trace(
			  TRACE_LEVEL_ERROR,
			  TRACE_REPORTING,
			  "Fts521ChangeSleepState - Unknown sleep state 0x%02X",
			  SleepState);
		  status = STATUS_INVALID_PARAMETER;
		  break;
	  }

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ChangeSleepState - Exit - 0x%08lX",
		  status);

	  return status;
}

NTSTATUS
Fts521FlushFifo(
	IN SPB_CONTEXT* SpbContext
)
{
	  BYTE setting;
	  NTSTATUS status;

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521FlushFifo - Entry");

	  setting = SPECIAL_FIFO_FLUSH;
	  status = Fts521WriteSystemCommand(
		  SpbContext,
		  SYS_CMD_SPECIAL,
		  &setting,
		  sizeof(setting));
	  if (!NT_SUCCESS(status))
	  {
		  Trace(
			  TRACE_LEVEL_ERROR,
			  TRACE_REPORTING,
			  "Fts521FlushFifo - System FIFO flush failed - 0x%08lX",
			  status);
	  }

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521FlushFifo - Exit - 0x%08lX",
		  status);

	  return status;
}

NTSTATUS
Fts521ConfigureInterruptEnable(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext
)
{
	  UNREFERENCED_PARAMETER(SpbContext);
	  UNREFERENCED_PARAMETER(ControllerContext);

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ConfigureInterruptEnable - Entry");

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ConfigureInterruptEnable - Host interrupt is configured by ACPI/KMDF");

	  return STATUS_SUCCESS;
}
