/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Sample code. Dealpoint ID #843729.

	Module Name:

		fts521pointer.h

	Abstract:

		Contains common types and defintions used internally
		by the multi touch screen driver.

	Environment:

		Kernel mode

	Revision History:

--*/

#pragma once

#include <fts521\fts521internal.h>

NTSTATUS
Fts521ProcessEnterOrMotionPointerEvent(
	FTS521_CONTROLLER_CONTEXT* ControllerContext,
	PREPORT_CONTEXT ReportContext,
	BYTE* EventData
);

NTSTATUS
Fts521ProcessLeavePointerEvent(
	FTS521_CONTROLLER_CONTEXT* ControllerContext,
	PREPORT_CONTEXT ReportContext,
	BYTE* EventData
);