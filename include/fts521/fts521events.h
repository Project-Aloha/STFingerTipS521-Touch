/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Sample code. Dealpoint ID #843729.

	Module Name:

		fts521events.h

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
TchServiceObjectInterrupts(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN PREPORT_CONTEXT ReportContext
);