/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Sample code. Dealpoint ID #843729.

	Module Name:

		fts521core.h

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
SetScanMode(
	SPB_CONTEXT* SpbContext,
	BYTE Mode,
	BYTE Settings
);