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
#include <fts521internal.tmh>

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
	WdfWaitLockAcquire(controller->ControllerLock, NULL);

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
		  TRACE_LEVEL_INFORMATION,
		  TRACE_REPORTING,
		  "Fts521BuildFunctionsTable - Entry");

	  Trace(
		  TRACE_LEVEL_INFORMATION,
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
		  TRACE_LEVEL_INFORMATION,
		  TRACE_REPORTING,
		  "Fts521ChangePage - Entry");

	  Trace(
		  TRACE_LEVEL_INFORMATION,
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
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ConfigureFunctions - Entry");

	LARGE_INTEGER delay;

	FTS521_CONTROLLER_CONTEXT* controller;
	controller = (FTS521_CONTROLLER_CONTEXT*)ControllerContext;

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
			"Fts521ConfigureFunctions - Error Writing Lockdown code into the IC");
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
		}
	}

	//Active Scan OFF
	Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, 0x00);

	delay.QuadPart = RELATIVE(MILLISECONDS(50));
	KeDelayExecutionThread(KernelMode, TRUE, &delay);

	//Active Scan ON
	Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, 0x01);

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ConfigureFunctions - Exit");

	return STATUS_SUCCESS;
}


NTSTATUS
Fts521SetScanMode(
	SPB_CONTEXT* SpbContext,
	BYTE Mode,
	BYTE Settings)
{
	NTSTATUS status;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_INTERRUPT,
		"Fts521SetScanMode - Entry");


	// FTS521_SCAN_MODE: 
	// * Address: FTS_CMD_SCAN_MODE
	// * { FTS_CMD_SCAN_MODE, Mode, Settings };
	BYTE FTS521_SCAN_MODE[2] = { Mode, Settings };
	status = SpbWriteDataSynchronously(SpbContext, FTS_CMD_SCAN_MODE, FTS521_SCAN_MODE, sizeof(FTS521_SCAN_MODE));

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Fts521SetScanMode - Setting scan mode Error");
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_INTERRUPT,
		"Fts521SetScanMode - Exit");

	return STATUS_SUCCESS;
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
		  TRACE_LEVEL_INFORMATION,
		  TRACE_REPORTING,
		  "Fts521SetReportingFlags - Entry");

	  Trace(
		  TRACE_LEVEL_INFORMATION,
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
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ChangeChargerConnectedState - Entry");

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ChangeChargerConnectedState - Exit");

	return STATUS_SUCCESS;
}

NTSTATUS
Fts521ChangeSleepState(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR SleepState
)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(ControllerContext);

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ChangeSleepState - Entry");

	if (SleepState == FTS521_F01_DEVICE_CONTROL_SLEEP_MODE_SLEEPING)
	{
		//
		// Put the IC into low-power scan mode to stop generating interrupts
		// and reduce power consumption during standby or system shutdown.
		//
		status = Fts521SetScanMode(SpbContext, SCAN_MODE_LOW_POWER, 0x00);
	}
	else
	{
		//
		// Resume active scanning.
		//
		status = Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, 0x01);
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ChangeSleepState - Exit - 0x%08lX",
		status);

	return status;
}

NTSTATUS
Fts521CheckInterrupts(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN ULONG* InterruptStatus
)
{
	  UNREFERENCED_PARAMETER(SpbContext);
	  UNREFERENCED_PARAMETER(ControllerContext);
	  UNREFERENCED_PARAMETER(InterruptStatus);

	  Trace(
		  TRACE_LEVEL_INFORMATION,
		  TRACE_REPORTING,
		  "Fts521CheckInterrupts - Entry");

	  Trace(
		  TRACE_LEVEL_INFORMATION,
		  TRACE_REPORTING,
		  "Fts521CheckInterrupts - Exit");

	  return STATUS_SUCCESS;
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
		  TRACE_LEVEL_INFORMATION,
		  TRACE_REPORTING,
		  "Fts521ConfigureInterruptEnable - Entry");

	  Trace(
		  TRACE_LEVEL_INFORMATION,
		  TRACE_REPORTING,
		  "Fts521ConfigureInterruptEnable - Exit");

	  return STATUS_SUCCESS;
}

NTSTATUS
Fts521ReadFirmwareVersion(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext
)
/*++

  Routine Description:

	Reads firmware version information from the IC system info framebuffer.
	The IC is requested to reload its system info, then the framebuffer is
	read and parsed to extract the firmware version and config version.

	Protocol reference (from ST Android driver, fts_lib/ftsCore.c):
	  - Send FTS_CMD_SYSTEM (0xA4) + SYS_CMD_LOAD_DATA (0x06) + LOAD_SYS_INFO (0x01)
	  - Read SYS_INFO_SIZE (208) bytes from framebuffer at address 0xA6 0x00 0x00
	  - First byte read is a dummy byte (I2C interface)
	  - Header signature (0xA5) is at data[0] after dummy, i.e. sysInfoBuf[1]
	  - u16_fwVer  (little-endian) is at data offset 16, i.e. sysInfoBuf[17..18]
	  - u16_cfgVer (little-endian) is at data offset 20, i.e. sysInfoBuf[21..22]

  Arguments:

	ControllerContext - Touch controller context

	SpbContext - A pointer to the current I2C context

  Return Value:

	NTSTATUS indicating success or failure

--*/
{
	NTSTATUS status;
	LARGE_INTEGER delay;

	//
	// Request the IC to reload system info into the framebuffer.
	//
	BYTE loadSysInfoCmd[2] = { SYS_CMD_LOAD_DATA, LOAD_SYS_INFO };
	status = SpbWriteDataSynchronously(SpbContext, FTS_CMD_SYSTEM, loadSysInfoCmd, sizeof(loadSysInfoCmd));

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Fts521ReadFirmwareVersion - Error sending load sys info command - 0x%08lX",
			status);
		return status;
	}

	//
	// Wait for the IC to load system info into the framebuffer.
	//
	delay.QuadPart = RELATIVE(MILLISECONDS(50));
	KeDelayExecutionThread(KernelMode, TRUE, &delay);

	//
	// Read the framebuffer. For I2C, the first byte returned is a dummy byte,
	// so SYS_INFO_BUF_SIZE = SYS_INFO_SIZE + 1.
	//
	BYTE framebufferCmd[3] = { FTS_CMD_FRAMEBUFFER_R, ADDR_FRAMEBUFFER_HIGH, ADDR_FRAMEBUFFER_LOW };
	BYTE sysInfoBuf[SYS_INFO_BUF_SIZE];
	RtlZeroMemory(sysInfoBuf, sizeof(sysInfoBuf));

	status = FtsWriteReadU8UX(SpbContext, framebufferCmd, sizeof(framebufferCmd), sysInfoBuf, sizeof(sysInfoBuf));

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Fts521ReadFirmwareVersion - Error reading framebuffer - 0x%08lX",
			status);
		return status;
	}

	//
	// Validate the header signature. sysInfoBuf[0] is the dummy byte;
	// actual data starts at sysInfoBuf[1].
	//
	if (sysInfoBuf[1] != HEADER_SIGNATURE)
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INIT,
			"Fts521ReadFirmwareVersion - Invalid header signature: 0x%02X (expected 0x%02X)",
			sysInfoBuf[1],
			HEADER_SIGNATURE);
		return STATUS_DEVICE_DATA_ERROR;
	}

	//
	// Parse firmware version (u16_fwVer, little-endian) at SYS_INFO_FW_VER_OFFSET.
	// Parse config version (u16_cfgVer, little-endian) at SYS_INFO_CFG_VER_OFFSET.
	//
	ControllerContext->FirmwareInfo.FwVersion =
		(USHORT)(sysInfoBuf[SYS_INFO_FW_VER_OFFSET] | ((USHORT)sysInfoBuf[SYS_INFO_FW_VER_OFFSET + 1] << 8));

	ControllerContext->FirmwareInfo.CfgVersion =
		(USHORT)(sysInfoBuf[SYS_INFO_CFG_VER_OFFSET] | ((USHORT)sysInfoBuf[SYS_INFO_CFG_VER_OFFSET + 1] << 8));

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_INIT,
		"Fts521ReadFirmwareVersion - FW Version: 0x%04X, Config Version: 0x%04X",
		ControllerContext->FirmwareInfo.FwVersion,
		ControllerContext->FirmwareInfo.CfgVersion);

	return STATUS_SUCCESS;
}