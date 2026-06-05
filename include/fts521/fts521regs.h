/*++
	Module Name:

		fts521regs.h

	Abstract:

		Contains common types and defintions used internally
		by the multi touch screen driver.

	Environment:

		Kernel mode

--*/

#define FIFO_EVENT_SIZE					8

#define FTS_CMD_LOCKDOWN_ID				0x70
#define FTS_CMD_SYSTEM					0xA4

#define EVT_ID_NOEVENT					0x00
#define EVT_ID_CONTROLLER_READY			0x03
#define EVT_ID_ENTER_POINT				0x13
#define EVT_ID_MOTION_POINT				0x23
#define EVT_ID_LEAVE_POINT				0x33
#define EVT_ID_STATUS_UPDATE			0x43
#define EVT_ID_USER_REPORT				0x53
#define EVT_ID_DEBUG					0xE3
#define EVT_ID_ERROR					0xF3

#define EVT_TYPE_STATUS_ECHO			0x01
#define EVT_TYPE_ERROR_ESD				0xF0
#define EVT_TYPE_ERROR_WATCHDOG			0x06	

#define FTS_CMD_SCAN_MODE				0xA0
#define SCAN_MODE_ACTIVE				0x00
#define SCAN_MODE_LOW_POWER				0x01
#define SCAN_MODE_ACTIVE_SETTINGS_NONE	0x00
#define SCAN_MODE_ACTIVE_SETTINGS_ALL	0xFF

#define SYS_CMD_SPECIAL					0x00
#define SPECIAL_FIFO_FLUSH				0x01

#define FIFO_CMD_READONE				0x86

//
// Maximum number of remaining FIFO events that fit in the 256-byte EventBuffer
// after the first event (FIFO_EVENT_SIZE bytes) has already been read.
//
#define FIFO_MAX_REMAIN_EVENTS			((256 / FIFO_EVENT_SIZE) - 1)
