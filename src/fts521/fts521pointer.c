/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Sample code. Dealpoint ID #843729.

	Module Name:

		fts521pointer.c

	Abstract:

		Contains STFingerTipSTouch initialization code

	Environment:

		Kernel mode

	Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <spb.h>
#include <report.h>
#include <fts521\fts521internal.h>
#include <fts521\fts521regs.h>
#include <fts521\fts521events.h>
#include <fts521\fts521pointer.h>
#include <fts521pointer.tmh>


NTSTATUS
Fts521ProcessEnterOrMotionPointerEvent(
	FTS521_CONTROLLER_CONTEXT* ControllerContext,
	PREPORT_CONTEXT ReportContext,
	BYTE* EventData
)
{
	NTSTATUS status = STATUS_SUCCESS;

	int x, y;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ProcessEvent - Enter or Motion Pointer");

	BYTE touchId = (EventData[1] & 0xF0) >> 4;

	if (touchId >= MAX_TOUCHES)
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_REPORTING,
			"Fts521ProcessEvent - Invalid touch id %d",
			touchId);
		goto exit;
	}

	x = ((EventData[3] & 0x0F) << 8) | (EventData[2]);
	y = (EventData[4] << 4) | ((EventData[3] & 0xF0) >> 4);


	ControllerContext->DetectedObjects.States[touchId] = OBJECT_STATE_FINGER_PRESENT_WITH_ACCURATE_POS;
	ControllerContext->DetectedObjects.Positions[touchId].X = x;
	ControllerContext->DetectedObjects.Positions[touchId].Y = y;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ProcessEvent - Touch %d at (x=%d, y=%d)",
		touchId,
		x,
		y);

	status = ReportObjects(
		ReportContext,
		ControllerContext->DetectedObjects);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_VERBOSE,
			TRACE_SAMPLES,
			"Fts521ProcessEvent - Error while reporting objects - 0x%08lX",
			status);

		goto exit;
	}

exit:

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ProcessEvent - Exit - 0x%08lX",
		status);

	return status;
}

NTSTATUS
Fts521ProcessLeavePointerEvent(
	FTS521_CONTROLLER_CONTEXT* ControllerContext,
	PREPORT_CONTEXT ReportContext,
	BYTE* EventData
)
{
	NTSTATUS status = STATUS_SUCCESS;

	int x, y;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ProcessEvent - Leave Pointer");

	BYTE touchId = (EventData[1] & 0xF0) >> 4;

	if (touchId >= MAX_TOUCHES)
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_REPORTING,
			"Fts521ProcessEvent - Invalid touch id %d",
			touchId);
		goto exit;
	}

	x = ((EventData[3] & 0x0F) << 8) | (EventData[2]);
	y = (EventData[4] << 4) | ((EventData[3] & 0xF0) >> 4);

	ControllerContext->DetectedObjects.States[touchId] = OBJECT_STATE_NOT_PRESENT;
	ControllerContext->DetectedObjects.Positions[touchId].X = x;
	ControllerContext->DetectedObjects.Positions[touchId].Y = y;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ProcessEvent - Touch %d at (x=%d, y=%d) Left",
		touchId,
		x,
		y);

	status = ReportObjects(
		ReportContext,
		ControllerContext->DetectedObjects);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_VERBOSE,
			TRACE_SAMPLES,
			"Fts521ProcessEvent - Error while reporting objects - 0x%08lX",
			status);

		goto exit;
	}

exit:

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_REPORTING,
		"Fts521ProcessEvent - Exit - 0x%08lX",
		status);

	return status;
}
