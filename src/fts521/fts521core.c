/*++
	  Module Name:

			fts521core.c

	  Abstract:

			Contains STFingerTipSTouch Core code

	  Environment:

			Kernel mode

	  Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <fts521\fts521core.h>
#include <fts521\fts521regs.h>
#include <fts521core.tmh>

NTSTATUS 
SetScanMode(
	SPB_CONTEXT* SpbContext,
	BYTE Mode,
	BYTE Settings)
{
	NTSTATUS status;
	BYTE scanModeData[2];
	ULONG scanModeLength;

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
			"Setting scan mode unknown mode 0x%02X",
			Mode);
		return STATUS_INVALID_PARAMETER;
	}

	status = SpbWriteDataSynchronously(SpbContext, FTS_CMD_SCAN_MODE, scanModeData, scanModeLength);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Setting scan mode Error - 0x%08lX",
			status);
		goto exit;
	}

	Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_INTERRUPT,
			"Setting scan mode OK");

exit:
	return status;
}
