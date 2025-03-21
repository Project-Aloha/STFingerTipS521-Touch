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
#include <fts521\fts521core.h>
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
		TRACE_LEVEL_ERROR,
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
		TRACE_LEVEL_ERROR,
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

	FTS521_CONTROLLER_CONTEXT* controller;
	controller = (FTS521_CONTROLLER_CONTEXT*)ControllerContext;

	BYTE FTS521_LOCKDOWN[3] = { 0xA4, 0x06, FTS_CMD_LOCKDOWN_ID };
	status = FtsWrite(SpbContext, FTS521_LOCKDOWN, 3);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Fts521ConfigureFunctions - Error Writing Lockdown code into the IC");
	}

	//Active Scan OFF
	SetScanMode(SpbContext, SCAN_MODE_ACTIVE, 0x00);

	delay.QuadPart = RELATIVE(MILLISECONDS(50));
	KeDelayExecutionThread(KernelMode, TRUE, &delay);

	//Active Scan ON
	SetScanMode(SpbContext, SCAN_MODE_ACTIVE, 0x01);

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"Fts521ConfigureFunctions - Exit");

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
	  UNREFERENCED_PARAMETER(SpbContext);
	  UNREFERENCED_PARAMETER(ControllerContext);
	  UNREFERENCED_PARAMETER(SleepState);

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ChangeSleepState - Entry");

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ChangeSleepState - Exit");

	  return STATUS_SUCCESS;
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
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521CheckInterrupts - Entry");

	  Trace(
		  TRACE_LEVEL_ERROR,
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
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ConfigureInterruptEnable - Entry");

	  Trace(
		  TRACE_LEVEL_ERROR,
		  TRACE_REPORTING,
		  "Fts521ConfigureInterruptEnable - Exit");

	  return STATUS_SUCCESS;
}