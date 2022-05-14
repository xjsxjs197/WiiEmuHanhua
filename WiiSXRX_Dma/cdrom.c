/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

/*
* Handles all CD-ROM registers and functions.
*/

#include "cdrom.h"
#include "ppf.h"
#include "psxdma.h"
#include <ogc/lwp_watchdog.h>

/* CD-ROM magic numbers */
#define CdlSync        0
#define CdlNop         1
#define CdlSetloc      2
#define CdlPlay        3
#define CdlForward     4
#define CdlBackward    5
#define CdlReadN       6
#define CdlStandby     7
#define CdlStop        8
#define CdlPause       9
#define CdlInit        10 // 0xa
#define CdlMute        11 // 0xb
#define CdlDemute      12 // 0xc
#define CdlSetfilter   13 // 0xd
#define CdlSetmode     14 // 0xe
#define CdlGetmode     15 // 0xf
#define CdlGetlocL     16 // 0x10
#define CdlGetlocP     17 // 0x11
#define CdlReadT       18 // 0x12
#define CdlGetTN       19 // 0x13
#define CdlGetTD       20 // 0x14
#define CdlSeekL       21 // 0x15
#define CdlSeekP       22 // 0x16
#define CdlSetclock    23 // 0x17
#define CdlGetclock    24 // 0x18
#define CdlTest        25 // 0x19
#define CdlID          26 // 0x1a
#define CdlReadS       27 // 0x1b
#define CdlReset       28 // 0x1c
#define CdlGetQ        29 // 0x1d
#define CdlReadToc     30 // 0x1e

#define AUTOPAUSE	249
#define READ_ACK	250
#define READ		251
#define REPPLAY_ACK	252
#define REPPLAY		253
#define ASYNC		254
/* don't set 255, it's reserved */

char *CmdName[0x100]= {
    "CdlSync",     "CdlNop",       "CdlSetloc",  "CdlPlay",
    "CdlForward",  "CdlBackward",  "CdlReadN",   "CdlStandby",
    "CdlStop",     "CdlPause",     "CdlInit",    "CdlMute",
    "CdlDemute",   "CdlSetfilter", "CdlSetmode", "CdlGetmode",
    "CdlGetlocL",  "CdlGetlocP",   "CdlReadT",   "CdlGetTN",
    "CdlGetTD",    "CdlSeekL",     "CdlSeekP",   "CdlSetclock",
    "CdlGetclock", "CdlTest",      "CdlID",      "CdlReadS",
    "CdlReset",    NULL,           "CDlReadToc", NULL
};

unsigned char Test04[] = { 0 };
unsigned char Test05[] = { 0 };
unsigned char Test20[] = { 0x98, 0x06, 0x10, 0xC3 };
unsigned char Test22[] = { 0x66, 0x6F, 0x72, 0x20, 0x45, 0x75, 0x72, 0x6F };
unsigned char Test23[] = { 0x43, 0x58, 0x44, 0x32, 0x39 ,0x34, 0x30, 0x51 };

// cdr.Stat:
#define NoIntr		0
#define DataReady	1
#define Complete	2
#define Acknowledge	3
#define DataEnd		4
#define DiskError	5

/* Modes flags */
#define MODE_SPEED       (1<<7) // 0x80
#define MODE_STRSND      (1<<6) // 0x40 ADPCM on/off
#define MODE_SIZE_2340   (1<<5) // 0x20
#define MODE_SIZE_2328   (1<<4) // 0x10
#define MODE_SIZE_2048   (0<<4) // 0x00
#define MODE_SF          (1<<3) // 0x08 channel on/off
#define MODE_REPORT      (1<<2) // 0x04
#define MODE_AUTOPAUSE   (1<<1) // 0x02
#define MODE_CDDA        (1<<0) // 0x01

/* Status flags */
#define STATUS_PLAY      (1<<7) // 0x80
#define STATUS_SEEK      (1<<6) // 0x40
#define STATUS_READ      (1<<5) // 0x20
#define STATUS_SHELLOPEN (1<<4) // 0x10
#define STATUS_UNKNOWN3  (1<<3) // 0x08
#define STATUS_UNKNOWN2  (1<<2) // 0x04
#define STATUS_ROTATING  (1<<1) // 0x02
#define STATUS_ERROR     (1<<0) // 0x01

/* Errors */
#define ERROR_NOTREADY   (1<<7) // 0x80
#define ERROR_INVALIDCMD (1<<6) // 0x40
#define ERROR_INVALIDARG (1<<5) // 0x20

// 1x = 75 sectors per second
// PSXCLK = 1 sec in the ps
// so (PSXCLK / 75) / BIAS = cdr read time (linuzappz)
#define cdReadTime ((PSXCLK / 75) / BIAS)

enum drive_state {
	DRIVESTATE_STANDBY = 0,
	DRIVESTATE_LID_OPEN,
	DRIVESTATE_RESCAN_CD,
	DRIVESTATE_PREPARE_CD,
	DRIVESTATE_STOPPED,
};

// for cdr.Seeked
enum seeked_state {
	SEEK_PENDING = 0,
	SEEK_DONE = 1,
};

static struct CdrStat stat;
static struct SubQ *subq;

// cdrInterrupt
#define CDR_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDR); \
	psxRegs.intCycle[PSXINT_CDR].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDR].sCycle = psxRegs.cycle; \
}

// cdrReadInterrupt
#define CDREAD_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDREAD); \
	psxRegs.intCycle[PSXINT_CDREAD].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDREAD].sCycle = psxRegs.cycle; \
}

// cdrLidSeekInterrupt
#define CDRLID_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDRLID); \
	psxRegs.intCycle[PSXINT_CDRLID].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDRLID].sCycle = psxRegs.cycle; \
}

// cdrPlayInterrupt
#define CDRMISC_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDRPLAY); \
	psxRegs.intCycle[PSXINT_CDRPLAY].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDRPLAY].sCycle = psxRegs.cycle; \
}


#define StartReading(type) { \
   	cdr.Reading = type; \
  	cdr.FirstSector = 1; \
  	cdr.Readed = 0xff; \
	AddIrqQueue(READ_ACK, 0x800); \
}

#define StopReading() { \
	if (cdr.Reading) { \
		cdr.Reading = 0; \
		psxRegs.interrupt &= ~(1 << PSXINT_CDREAD); \
	} \
	cdr.StatP &= ~(STATUS_READ|STATUS_SEEK);\
}

#define StopCdda() { \
	if (cdr.Play) { \
		if (!Config.Cdda) CDR_stop(); \
		cdr.StatP &= ~STATUS_PLAY; \
		cdr.Play = 0; \
	} \
}

#define SetResultSize(size) { \
    cdr.ResultP = 0; \
	cdr.ResultC = size; \
	cdr.ResultReady = 1; \
}

static void setIrq(void)
{
	if (cdr.Stat & cdr.Reg2) {
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);
		psxRegs.interrupt|= 0x80000000;
	}
}

// timing used in this function was taken from tests on real hardware
// (yes it's slow, but you probably don't want to modify it)
void cdrLidSeekInterrupt()
{
	switch (cdr.DriveState) {
	default:
	    #ifdef DISP_DEBUG
        PRINT_LOG("cdrLidSeekInterrupt=default ");
        #endif // DISP_DEBUG
	case DRIVESTATE_STANDBY:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_STANDBY: %x ", stat.Status);
        #endif // DISP_DEBUG
		cdr.StatP &= ~STATUS_SEEK;

		if (CDR_getStatus(&stat) == -1)
			return;

        #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_STANDBY2: %x ", stat.Status);
        #endif // DISP_DEBUG
		if (stat.Status & STATUS_SHELLOPEN)
		{
			StopCdda();
			cdr.DriveState = DRIVESTATE_LID_OPEN;
			CDRLID_INT(0x800);
		}
		break;

	case DRIVESTATE_LID_OPEN:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_LID_OPEN: %x ", cdr.StatP);
        #endif // DISP_DEBUG
		if (CDR_getStatus(&stat) == -1)
			stat.Status &= ~STATUS_SHELLOPEN;

		// 02, 12, 10
		if (!(cdr.StatP & STATUS_SHELLOPEN)) {
			StopReading();
			cdr.StatP |= STATUS_SHELLOPEN;

			// could generate error irq here, but real hardware
			// only sometimes does that
			// (not done when lots of commands are sent?)

			CDRLID_INT(cdReadTime * 30);
			break;
		}
		else if (cdr.StatP & STATUS_ROTATING) {
			cdr.StatP &= ~STATUS_ROTATING;
		}
		else if (!(stat.Status & STATUS_SHELLOPEN)) {
			// closed now
			CheckCdrom();

			// cdr.StatP STATUS_SHELLOPEN is "sticky"
			// and is only cleared by CdlNop

			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			CDRLID_INT(cdReadTime * 105);
			break;
		}

		// recheck for close
		CDRLID_INT(cdReadTime * 3);
		break;

	case DRIVESTATE_RESCAN_CD:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_RESCAN_CD: %x ", cdr.StatP);
        #endif // DISP_DEBUG
		cdr.StatP |= STATUS_ROTATING;
		cdr.DriveState = DRIVESTATE_PREPARE_CD;

		// this is very long on real hardware, over 6 seconds
		// make it a bit faster here...
		CDRLID_INT(cdReadTime * 150);
		break;

	case DRIVESTATE_PREPARE_CD:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_PREPARE_CD: %x ", cdr.StatP);
        #endif // DISP_DEBUG
		cdr.StatP |= STATUS_SEEK;

		cdr.DriveState = DRIVESTATE_STANDBY;
		CDRLID_INT(cdReadTime * 26);
		break;
	}
}
// ReadTrack=========================
void ReadTrack() {
//    unsigned char tmp[3];
//	tmp[0] = itob(cdr.SetSector[0]);
//	tmp[1] = itob(cdr.SetSector[1]);
//	tmp[2] = itob(cdr.SetSector[2]);
//
//	if (cdr.Prev[0] == tmp[0] && cdr.Prev[1] == tmp[1] && cdr.Prev[2] == tmp[2])
//    {
//        return;
//    }
    if (cdr.Prev[0] == cdr.SetSector[0] && cdr.Prev[1] == cdr.SetSector[1] && cdr.Prev[2] == cdr.SetSector[2])
    {
        return;
    }
#ifdef CDR_LOG
	CDR_LOG("ReadTrack() Log: KEY *** %x:%x:%x\n", cdr.Prev[0], cdr.Prev[1], cdr.Prev[2]);
#endif
    #ifdef DISP_DEBUG
	//u64 start = ticks_to_nanosecs(gettick());
	#endif // DISP_DEBUG
	cdr.RErr = CDR_readTrack(cdr.SetSector);
	cdr.Prev[0] = cdr.SetSector[0];
	cdr.Prev[1] = cdr.SetSector[1];
	cdr.Prev[2] = cdr.SetSector[2];
	#ifdef DISP_DEBUG
    //u64 end = ticks_to_nanosecs(gettick());
	//PRINT_LOG1("CDR_readTrack=====%llu=", end - start);
	#endif // DISP_DEBUG
}


// AddIrqQueue=====================
void AddIrqQueue(unsigned char irq, unsigned long ecycle) {
	if (cdr.Irq != 0) {
		if (irq == cdr.Irq || irq + 0x100 == cdr.Irq) {
			cdr.IrqRepeated = 1;
			CDR_INT(ecycle);
			return;
		}

	}
	cdr.Irq = irq;
	if (cdr.Stat) {
		cdr.eCycle = ecycle;
	} else {
		CDR_INT(ecycle);
	}
}

// also handles seek
void cdrPlayInterrupt()
{
	if (cdr.Seeked == SEEK_PENDING) {
		if (cdr.Stat) {
			//CDR_LOG_I("cdrom: seek stat hack\n");
			CDRMISC_INT(0x800);
			return;
		}
		SetResultSize(1);
		cdr.StatP |= STATUS_ROTATING;
		cdr.StatP &= ~STATUS_SEEK;
		cdr.Result[0] = cdr.StatP;
		cdr.Seeked = SEEK_DONE;
		if (cdr.Irq == 0) {
			cdr.Stat = Complete;
			setIrq();
		}

		if (cdr.SetlocPending) {
			//memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
			//*((u32*)cdr.SetSectorPlay) = *((u32*)cdr.SetSector);
			cdr.SetlocPending = 0;
			cdr.m_locationChanged = TRUE;
		}
		//Find_CurTrack(cdr.SetSectorPlay);
		//ReadTrack(cdr.SetSectorPlay);
		ReadTrack();
		cdr.TrackChanged = FALSE;
	}

	if (!cdr.Play) return;

	cdr.SetSector[2]++;
	if (cdr.SetSector[2] == 75) {
		cdr.SetSector[2] = 0;
		cdr.SetSector[1]++;
		if (cdr.SetSector[1] == 60) {
			cdr.SetSector[1] = 0;
			cdr.SetSector[0]++;
		}
	}

	if (cdr.m_locationChanged)
	{
		CDRMISC_INT(cdReadTime * 30);
		cdr.m_locationChanged = FALSE;
	}
	else
	{
		CDRMISC_INT(cdReadTime);
	}

	// update for CdlGetlocP/autopause
	//generate_subq(cdr.SetSectorPlay);
}

void cdrInterrupt() {
	int no_busy_error = 0;
	int start_rotating = 0;
	int i;
	int delay;
	unsigned char Irq = cdr.Irq;

	if (cdr.Stat) {
		CDR_INT(0x800);
		return;
	}

	//cdr.Irq = 0xff;
	cdr.Ctrl&=~0x80;


	if (cdr.IrqRepeated) {
		cdr.IrqRepeated = 0;
		if (cdr.eCycle > psxRegs.cycle) {
		    SetResultSize(1);
	        cdr.Result[0] = cdr.StatP;
	        cdr.Stat = Acknowledge;

			CDR_INT(cdr.eCycle);
			setIrq();
	        cdr.ParamP = 0;
			return;
		}
	}

	cdr.Irq = 0xff;
	switch (Irq) {
    	case CdlSync:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			break;

    	case CdlNop:
			SetResultSize(1);
        	cdr.Result[0] = cdr.StatP;
			if (cdr.DriveState != DRIVESTATE_LID_OPEN)
				cdr.StatP &= ~STATUS_SHELLOPEN;
			no_busy_error = 1;
        	cdr.Stat = Acknowledge;
			i = stat.Status;
        	if (CDR_getStatus(&stat) != -1) {
				if (stat.Type == 0xff) cdr.Stat = DiskError;
				if (stat.Status & 0x10) {
					cdr.Stat = DiskError;
					cdr.Result[0]|= 0x11;
					cdr.Result[0]&=~0x02;
				}
				else if (i & 0x10) {
					cdr.StatP |= 0x2;
					cdr.Result[0]|= 0x2;
					CheckCdrom();
				}
			}
			break;

		case CdlSetloc:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			break;

		case CdlPlay:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP|= 0x2;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Acknowledge;
			cdr.StatP|= 0x80;
//			if ((cdr.Mode & 0x5) == 0x5) AddIrqQueue(REPPLAY, cdReadTime);
			break;

    	case CdlForward:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Complete;
			break;

    	case CdlBackward:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Complete;
			break;

    	case CdlStandby:
			cdr.CmdProcess = 0;
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	//cdr.Stat = Complete;
			AddIrqQueue(CdlStandby + 0x100, cdReadTime * 125 / 2);
			break;

		case CdlStandby + 0x100:
			cdr.Stat = Complete;
			break;

		case CdlStop:
			cdr.CmdProcess = 0;
			SetResultSize(1);
        	cdr.StatP&=~0x2;
			cdr.Result[0] = cdr.StatP;
        	//cdr.Stat = Complete;
			delay = 0x800;
			if (cdr.DriveState == DRIVESTATE_STANDBY)
				delay = cdReadTime * 30 / 2;

			cdr.DriveState = DRIVESTATE_STOPPED;
			AddIrqQueue(CdlStop + 0x100, delay);
//        	cdr.Stat = Acknowledge;
			break;

		case CdlStop + 0x100:
			cdr.StatP &= ~STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;

		case CdlPause:
			SetResultSize(1);
			cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
            /*
			Gundam Battle Assault 2: much slower (*)
			- Fixes boot, gameplay

			Hokuto no Ken 2: slower
			- Fixes intro + subtitles

			InuYasha - Feudal Fairy Tale: slower
			- Fixes battles
			*/
			/* Gameblabla - Tightening the timings (as taken from Duckstation).
			 * The timings from Duckstation are based upon hardware tests.
			 * Mednafen's timing don't work for Gundam Battle Assault 2 in PAL/50hz mode,
			 * seems to be timing sensitive as it can depend on the CPU's clock speed.
			 *
			 * We will need to get around this for Bedlam/Rise 2 later...
			 * */
			if (cdr.DriveState != DRIVESTATE_STANDBY)
			{
				delay = 7000;
			}
			else
			{
				delay = (((cdr.Mode & MODE_SPEED) ? 2 : 1) * (1000000));
				CDRMISC_INT((cdr.Mode & MODE_SPEED) ? cdReadTime / 2 : cdReadTime);
			}
			AddIrqQueue(CdlPause + 0x20, delay >> 1);
			cdr.Ctrl|= 0x80;
			break;

		case CdlPause + 0x20:
			SetResultSize(1);
        	cdr.StatP&=~0x20;
			cdr.StatP|= 0x2;
			cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Complete;
			break;

    	case CdlInit:
			SetResultSize(1);
        	cdr.StatP = 0x2;
			cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
//			if (!cdr.Init) {
				AddIrqQueue(CdlInit + 0x20, 0x800);
//			}
            no_busy_error = 1;
			start_rotating = 1;
        	break;

		case CdlInit + 0x20:
			SetResultSize(1);
			cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Complete;
			cdr.Init = 1;
			break;

    	case CdlMute:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			break;

    	case CdlDemute:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			break;

    	case CdlSetfilter:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
        	break;

		case CdlSetmode:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			no_busy_error = 1;
        	break;

    	case CdlGetmode:
			SetResultSize(6);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Result[1] = cdr.Mode;
        	cdr.Result[2] = cdr.File;
        	cdr.Result[3] = cdr.Channel;
        	cdr.Result[4] = 0;
        	cdr.Result[5] = 0;
        	cdr.Stat = Acknowledge;
			no_busy_error = 1;
        	break;

    	case CdlGetlocL:
			SetResultSize(8);
//        	for (i=0; i<8; i++) cdr.Result[i] = itob(cdr.Transfer[i]);
        	for (i=0; i<8; i++) cdr.Result[i] = cdr.Transfer[i];
        	cdr.Stat = Acknowledge;
        	break;

    	case CdlGetlocP:
			SetResultSize(8);
			subq = (struct SubQ*) CDR_getBufferSub();
			if (subq != NULL) {
				cdr.Result[0] = subq->TrackNumber;
				cdr.Result[1] = subq->IndexNumber;
		    	memcpy(cdr.Result+2, subq->TrackRelativeAddress, 3);
		    	memcpy(cdr.Result+5, subq->AbsoluteAddress, 3);
			} else {
	        	cdr.Result[0] = 1;
	        	cdr.Result[1] = 1;
	        	cdr.Result[2] = itob(cdr.Prev[0]);
	        	cdr.Result[3] = itob((btoi(itob(cdr.Prev[1]))) - 2);
	        	//cdr.Result[3] = itob(cdr.Prev[1]);
	        	cdr.Result[4] = itob(cdr.Prev[2]);
		    	//memcpy(cdr.Result+5, cdr.Prev, 3);
		    	cdr.Result[5] = cdr.Result[2];
		    	cdr.Result[6] = cdr.Result[3];
		    	cdr.Result[7] = cdr.Result[4];
			}
        	cdr.Stat = Acknowledge;
        	break;

    	case CdlGetTN:
			cdr.CmdProcess = 0;
			SetResultSize(3);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	if (CDR_getTN(cdr.ResultTN) == -1) {
				cdr.Stat = DiskError;
				cdr.Result[0]|= 0x01;
        	} else {
        		cdr.Stat = Acknowledge;
        	    cdr.Result[1] = itob(cdr.ResultTN[0]);
        	    cdr.Result[2] = itob(cdr.ResultTN[1]);
        	}
        	break;

    	case CdlGetTD:
			cdr.CmdProcess = 0;
        	cdr.Track = btoi(cdr.Param[0]);
			SetResultSize(4);
			cdr.StatP|= 0x2;
        	if (CDR_getTD(cdr.Track, cdr.ResultTD) == -1) {
				cdr.Stat = DiskError;
				cdr.Result[0]|= 0x01;
        	} else {
        		cdr.Stat = Acknowledge;
				cdr.Result[0] = cdr.StatP;
	    		cdr.Result[1] = itob(cdr.ResultTD[2]);
        	    cdr.Result[2] = itob(cdr.ResultTD[1]);
				cdr.Result[3] = itob(cdr.ResultTD[0]);
	    	}
			break;

    	case CdlSeekL:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
			cdr.StatP|= 0x40;
        	cdr.Stat = Acknowledge;
			cdr.Seeked = 1;
			AddIrqQueue(CdlSeekL + 0x20, 0x800);
			break;

    	case CdlSeekL + 0x20:
			SetResultSize(1);
			cdr.StatP|= 0x2;
			cdr.StatP&=~0x40;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Complete;
			break;

    	case CdlSeekP:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
			cdr.StatP|= 0x40;
        	cdr.Stat = Acknowledge;
			AddIrqQueue(CdlSeekP + 0x20, 0x800);
			break;

    	case CdlSeekP + 0x20:
			SetResultSize(1);
			cdr.StatP|= 0x2;
			cdr.StatP&=~0x40;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Complete;
			break;

		case CdlTest:
        	cdr.Stat = Acknowledge;
        	switch (cdr.Param[0]) {
        	    case 0x20: // System Controller ROM Version
					SetResultSize(4);
					memcpy(cdr.Result, Test20, 4);
					break;
				case 0x22:
					SetResultSize(8);
					memcpy(cdr.Result, Test22, 4);
					break;
				case 0x23: case 0x24:
					SetResultSize(8);
					memcpy(cdr.Result, Test23, 4);
					break;
			}
			no_busy_error = 1;
			break;

    	case CdlID:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			AddIrqQueue(CdlID + 0x20, 0x800);
			break;

		case CdlID + 0x20:
			SetResultSize(8);
        	if (CDR_getStatus(&stat) == -1) {
        		cdr.Result[0] = 0x00;         // 0x00 Game CD
                cdr.Result[1] = 0x00;     // 0x00 loads CD
        	}
        	else {
                if (stat.Type == 2) {
                	cdr.Result[0] = 0x08;   // 0x08 audio cd
                    cdr.Result[1] = 0x10; // 0x10 enter cd player
	        	}
	        	else {
                    cdr.Result[0] = 0x00; // 0x00 game CD
                    cdr.Result[1] = 0x00; // 0x00 loads CD
	        	}
        	}
        	if (!LoadCdBios) cdr.Result[1] |= 0x80; //0x80 leads to the menu in the bios

        	cdr.Result[2] = 0x00;
        	cdr.Result[3] = 0x00;
//			strncpy((char *)&cdr.Result[4], "PCSX", 4);
#ifdef HW_RVL
			strncpy((char *)&cdr.Result[4], "WSX ", 4);
#else
			strncpy((char *)&cdr.Result[4], "GCSX", 4);
#endif
			cdr.Stat = Complete;
			break;

		case CdlReset:
			SetResultSize(1);
        	cdr.StatP = 0x2;
			// yes, it really sets STATUS_SHELLOPEN
			cdr.StatP |= STATUS_SHELLOPEN;
			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			CDRLID_INT(20480);
			no_busy_error = 1;
			start_rotating = 1;
			cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			break;

		case CdlGetQ:
			no_busy_error = 1;
			break;

    	case CdlReadToc:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Acknowledge;
			AddIrqQueue(CdlReadToc + 0x20, 0x800);
			no_busy_error = 1;
			start_rotating = 1;
			break;

    	case CdlReadToc + 0x20:
			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
        	cdr.Stat = Complete;
			no_busy_error = 1;
			break;

		case AUTOPAUSE:
			cdr.OCUP = 0;
/*			SetResultSize(1);
			StopCdda();
			StopReading();
			cdr.OCUP = 0;
        	cdr.StatP&=~0x20;
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
    		cdr.Stat = DataEnd;
*/			AddIrqQueue(CdlPause, 0x400);
			break;

		case READ_ACK:
			if (!cdr.Reading) return;

			SetResultSize(1);
			cdr.StatP|= 0x2;
        	cdr.Result[0] = cdr.StatP;
			if (cdr.Seeked == 0) {
				cdr.Seeked = 1;
				cdr.StatP|= 0x40;
			}
			cdr.StatP|= 0x20;
        	cdr.Stat = Acknowledge;

			ReadTrack();

//			CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
			CDREAD_INT(0x40000);
			break;

		case REPPLAY_ACK:
			cdr.Stat = Acknowledge;
			cdr.Result[0] = cdr.StatP;
			SetResultSize(1);
			AddIrqQueue(REPPLAY, cdReadTime);
			break;

		case REPPLAY:
			if ((cdr.Mode & 5) != 5) break;
/*			if (CDR_getStatus(&stat) == -1) {
				cdr.Result[0] = 0;
				cdr.Result[1] = 0;
				cdr.Result[2] = 0;
				cdr.Result[3] = 0;
				cdr.Result[4] = 0;
				cdr.Result[5] = 0;
				cdr.Result[6] = 0;
				cdr.Result[7] = 0;
			} else memcpy(cdr.Result, &stat.Track, 8);
			cdr.Stat = 1;
			SetResultSize(8);
			AddIrqQueue(REPPLAY_ACK, cdReadTime);
*/			break;

		case 0xff:
			return;

		default:
			cdr.Stat = Complete;
			break;
	}

	if (cdr.DriveState == DRIVESTATE_STOPPED && start_rotating) {
		cdr.DriveState = DRIVESTATE_STANDBY;
		cdr.StatP |= STATUS_ROTATING;
	}

	if (!no_busy_error) {
		switch (cdr.DriveState) {
		case DRIVESTATE_LID_OPEN:
		case DRIVESTATE_RESCAN_CD:
		case DRIVESTATE_PREPARE_CD:
			SetResultSize(2);
			cdr.Result[0] = cdr.StatP | STATUS_ERROR;
			cdr.Result[1] = ERROR_NOTREADY;
			cdr.Stat = DiskError;
			break;
		}
	}

	if (cdr.Stat != NoIntr && cdr.Reg2 != 0x18) {
		psxHu32ref(0x1070)|= SWAP32((u32)0x4);
		psxRegs.interrupt|= 0x80000000;
	}

#ifdef CDR_LOG
	CDR_LOG("cdrInterrupt() Log: CDR Interrupt IRQ %x\n", Irq);
#endif
}

void cdrReadInterrupt() {
	u8 *buf;

	if (!cdr.Reading)
		return;

	if (cdr.Irq || cdr.Stat) {
		CDREAD_INT(0x800);
		return;
	}

#ifdef CDR_LOG
	CDR_LOG("cdrReadInterrupt() Log: KEY END");
#endif

    cdr.OCUP = 1;
	SetResultSize(1);
	cdr.StatP |= STATUS_READ|STATUS_ROTATING;
	cdr.StatP &= ~STATUS_SEEK;
    cdr.Result[0] = cdr.StatP;

	buf = CDR_getBuffer();
	if (buf == NULL) {
		cdr.RErr = -1;
#ifdef CDR_LOG
		fprintf(emuLog, "cdrReadInterrupt() Log: err\n");
#endif
		memset(cdr.Transfer, 0, DATA_SIZE);
		cdr.Stat = DiskError;
		cdr.Result[0] |= STATUS_ERROR;
		ReadTrack();
		CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
		return;
	}

	cacheable_kernel_memcpy(cdr.Transfer, buf, DATA_SIZE);
    cdr.Stat = DataReady;

#ifdef CDR_LOG
	fprintf(emuLog, "cdrReadInterrupt() Log: cdr.Transfer %x:%x:%x\n", cdr.Transfer[0], cdr.Transfer[1], cdr.Transfer[2]);
#endif

	if ((cdr.Muted == 1) && (cdr.Mode & MODE_STRSND) && (!Config.Xa) && (cdr.FirstSector != -1)) { // CD-XA
		// Firemen 2: Multi-XA files - briefings, cutscenes
		if( cdr.FirstSector == 1 && (cdr.Mode & MODE_SF)==0 ) {
			cdr.File = cdr.Transfer[4 + 0];
			cdr.Channel = cdr.Transfer[4 + 1];
		}
		/* Gameblabla
		 * Skips playing on channel 255.
		 * Fixes missing audio in Blue's Clues : Blue's Big Musical. (Should also fix Taxi 2)
		 * TODO : Check if this is the proper behaviour.
		 * */
		if((cdr.Transfer[4 + 2] & 0x4) &&
			((cdr.Mode & MODE_SF) ? (cdr.Transfer[4+1] == cdr.Channel) : 1) &&
			 (cdr.Transfer[4 + 0] == cdr.File) && cdr.Channel != 255) {
			int ret = xa_decode_sector(&cdr.Xa, cdr.Transfer+4, cdr.FirstSector);
			if (!ret) {
				SPU_playADPCMchannel(&cdr.Xa);
				cdr.FirstSector = 0;
			}
			else cdr.FirstSector = -1;
		}
	}

	cdr.SetSector[2]++;
    if (cdr.SetSector[2] == 75) {
        cdr.SetSector[2] = 0;
        cdr.SetSector[1]++;
        if (cdr.SetSector[1] == 60) {
            cdr.SetSector[1] = 0;
            cdr.SetSector[0]++;
        }
    }

    cdr.Readed = 0;

	if ((cdr.Transfer[4+2] & 0x80) && (cdr.Mode & 0x2)) { // EOF
#ifdef CDR_LOG
		CDR_LOG("cdrReadInterrupt() Log: Autopausing read\n");
#endif
//		AddIrqQueue(AUTOPAUSE, 0x800);
		AddIrqQueue(CdlPause, 0x800);
	}
	else {
		ReadTrack();
		CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
	}
	psxHu32ref(0x1070)|= SWAP32((u32)0x4);
	psxRegs.interrupt|= 0x80000000;
}

/*
cdrRead0:
	bit 0 - 0 REG1 command send / 1 REG1 data read
	bit 1 - 0 data transfer finish / 1 data transfer ready/in progress
	bit 2 - unknown
	bit 3 - unknown
	bit 4 - unknown
	bit 5 - 1 result ready
	bit 6 - 1 dma ready
	bit 7 - 1 command being processed
*/

unsigned char cdrRead0(void) {
	if (cdr.ResultReady)
		cdr.Ctrl |= 0x20;
	else
		cdr.Ctrl &= ~0x20;

	if (cdr.OCUP)
		cdr.Ctrl |= 0x40;
//  else
//		cdr.Ctrl &= ~0x40;

	// What means the 0x10 and the 0x08 bits? I only saw it used by the bios
	cdr.Ctrl |= 0x18;

#ifdef CDR_LOG
	CDR_LOG("cdrRead0() Log: CD0 Read: %x\n", cdr.Ctrl);
#endif
	return psxHu8(0x1800) = cdr.Ctrl;
}

/*
cdrWrite0:
	0 - to send a command / 1 - to get the result
*/

void cdrWrite0(unsigned char rt) {
#ifdef CDR_LOG
	CDR_LOG("cdrWrite0() Log: CD0 write: %x\n", rt);
#endif
	cdr.Ctrl = (rt & 3) | (cdr.Ctrl & ~3);

    if (rt == 0) {
		cdr.ParamP = 0;
		cdr.ParamC = 0;
		cdr.ResultReady = 0;
	}
}

unsigned char cdrRead1(void) {
	if ((cdr.ResultP & 0xf) < cdr.ResultC)
		psxHu8(0x1801) = cdr.Result[cdr.ResultP & 0xf];
	else
		psxHu8(0x1801) = 0;
	cdr.ResultP++;
	if (cdr.ResultP == cdr.ResultC)
		cdr.ResultReady = 0;

#ifdef CDR_LOG
	CDR_LOG("cdrRead1() Log: CD1 Read: %x\n", psxHu8(0x1801));
#endif
	return psxHu8(0x1801);
}

void cdrWrite1(unsigned char rt) {
	u8 set_loc[3];
	int i;

#ifdef CDR_LOG
	CDR_LOG("cdrWrite1() Log: CD1 write: %x (%s)\n", rt, CmdName[rt]);
#endif
//	psxHu8(0x1801) = rt;
    cdr.Cmd = rt;
	cdr.OCUP = 0;

#ifdef CDRCMD_DEBUG
	SysPrintf("cdrWrite1() Log: CD1 write: %x (%s)", rt, CmdName[rt]);
	if (cdr.ParamC) {
		SysPrintf(" Param[%d] = {", cdr.ParamC);
		for (i=0;i<cdr.ParamC;i++) SysPrintf(" %x,", cdr.Param[i]);
		SysPrintf("}\n");
	} else SysPrintf("\n");
#endif

	if (cdr.Ctrl & 0x1) return;

	cdr.ResultReady = 0;

// cdrWrite1 switch =====================================
    switch(cdr.Cmd) {
    	case CdlSync:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlNop:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlSetloc:
			/*StopReading();
			cdr.Seeked = 0;
        	for (i=0; i<3; i++) cdr.SetSector[i] = btoi(cdr.Param[i]);
        	cdr.SetSector[3] = 0;*/
/*        	if ((cdr.SetSector[0] | cdr.SetSector[1] | cdr.SetSector[2]) == 0) {
				*(u32 *)cdr.SetSector = *(u32 *)cdr.SetSectorSeek;
			}*/

            // MM must be BCD, SS must be BCD and <0x60, FF must be BCD and <0x75
            if (((cdr.Param[0] & 0x0F) > 0x09) || (cdr.Param[0] > 0x99) || ((cdr.Param[1] & 0x0F) > 0x09) || (cdr.Param[1] >= 0x60) || ((cdr.Param[2] & 0x0F) > 0x09) || (cdr.Param[2] >= 0x75))
            {
                //CDR_LOG("Invalid/out of range seek to %02X:%02X:%02X\n", cdr.Param[0], cdr.Param[1], cdr.Param[2]);
            }
            else
            {
                for (i = 0; i < 3; i++)
                {
                    set_loc[i] = btoi(cdr.Param[i]);
                }

//                i = msf2sec(cdr.SetSectorPlay);
//                i = abs(i - msf2sec(set_loc));
//                if (i > 16)
//                    cdr.Seeked = SEEK_PENDING;

                memcpy(cdr.SetSector, set_loc, 3);
                cdr.SetSector[3] = 0;
                //cdr.SetlocPending = 1;
            }
			cdr.Ctrl|= 0x80;
        	//cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlPlay:
        	if (!cdr.SetSector[0] && !cdr.SetSector[1] && !cdr.SetSector[2]) {
            	if (CDR_getTN(cdr.ResultTN) != -1) {
	                if (cdr.CurTrack > cdr.ResultTN[1]) cdr.CurTrack = cdr.ResultTN[1];
                    if (CDR_getTD((unsigned char)(cdr.CurTrack), cdr.ResultTD) != -1) {
		               	int tmp = cdr.ResultTD[2];
                        cdr.ResultTD[2] = cdr.ResultTD[0];
						cdr.ResultTD[0] = tmp;
	                    if (!Config.Cdda) CDR_play(cdr.ResultTD);
					}
                }
			}
    		else if (!Config.Cdda) CDR_play(cdr.SetSector);
    		cdr.Play = 1;
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
    		break;

    	case CdlForward:
        	if (cdr.CurTrack < 0xaa) cdr.CurTrack++;
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlBackward:
        	if (cdr.CurTrack > 1) cdr.CurTrack--;
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlReadN:
			cdr.Irq = 0;
			StopReading();
			cdr.Ctrl|= 0x80;
        	cdr.Stat = NoIntr;
			StartReading(1);
        	break;

    	case CdlStandby:
			StopCdda();
			StopReading();
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlStop:
			StopCdda();
			StopReading();
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlPause:
			StopCdda();
			StopReading();
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x40000);
        	break;

		case CdlReset:
    	case CdlInit:
			StopCdda();
			StopReading();
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlMute:
        	cdr.Muted = 0;
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlDemute:
        	cdr.Muted = 1;
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlSetfilter:
        	cdr.File = cdr.Param[0];
        	cdr.Channel = cdr.Param[1];
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlSetmode:
#ifdef CDR_LOG
			CDR_LOG("cdrWrite1() Log: Setmode %x\n", cdr.Param[0]);
#endif
        	cdr.Mode = cdr.Param[0];
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlGetmode:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlGetlocL:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlGetlocP:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
			AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlGetTN:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlGetTD:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlSeekL:
//			((u32 *)cdr.SetSectorSeek)[0] = ((u32 *)cdr.SetSector)[0];
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlSeekP:
//        	((u32 *)cdr.SetSectorSeek)[0] = ((u32 *)cdr.SetSector)[0];
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlTest:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlID:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	case CdlReadS:
			cdr.Irq = 0;
			StopReading();
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
			StartReading(2);
        	break;

    	case CdlReadToc:
			cdr.Ctrl|= 0x80;
    		cdr.Stat = NoIntr;
    		AddIrqQueue(cdr.Cmd, 0x800);
        	break;

    	default:
#ifdef CDR_LOG
			CDR_LOG("cdrWrite1() Log: Unknown command: %x\n", cdr.Cmd);
#endif
			return;
    }
	if (cdr.Stat != NoIntr) {
		psxHu32ref(0x1070)|= SWAP32((u32)0x4);
		psxRegs.interrupt|= 0x80000000;
	}
}

unsigned char cdrRead2(void) {
	unsigned char ret;

	if (cdr.Readed == 0) {
		ret = 0;
	} else {
		ret = *cdr.pTransfer++;
	}

#ifdef CDR_LOG
	CDR_LOG("cdrRead2() Log: CD2 Read: %x\n", ret);
#endif
	return ret;
}

void cdrWrite2(unsigned char rt) {
#ifdef CDR_LOG
	CDR_LOG("cdrWrite2() Log: CD2 write: %x\n", rt);
#endif
    if (cdr.Ctrl & 0x1) {
		switch (rt) {
			case 0x07:
	    		cdr.ParamP = 0;
				cdr.ParamC = 0;
				cdr.ResultReady = 1; //0;
				cdr.Ctrl&= ~3; //cdr.Ctrl = 0;
				break;

			default:
				cdr.Reg2 = rt;
				setIrq();
				break;
		}
    } else if (!(cdr.Ctrl & 0x1) && cdr.ParamP < 8) {
		cdr.Param[cdr.ParamP++] = rt;
		cdr.ParamC++;
	}
}

unsigned char cdrRead3(void) {
	if (cdr.Stat) {
		if (cdr.Ctrl & 0x1) psxHu8(0x1803) = cdr.Stat | 0xE0;
		else psxHu8(0x1803) = cdr.Reg2 | 0xE0;
	} else psxHu8(0x1803) = 0;
#ifdef CDR_LOG
	CDR_LOG("cdrRead3() Log: CD3 Read: %x\n", psxHu8(0x1803));
#endif
	return psxHu8(0x1803);
}

void cdrWrite3(unsigned char rt) {
#ifdef CDR_LOG
	CDR_LOG("cdrWrite3() Log: CD3 write: %x\n", rt);
#endif
    if (rt == 0x07 && cdr.Ctrl & 0x1) {
		cdr.Stat = 0;

		if (cdr.Irq == 0xff) { cdr.Irq = 0; return; }
        if (cdr.Irq) CDR_INT(cdr.eCycle);
        if (cdr.Reading && !cdr.ResultReady)
            CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);

		return;
	}

	if ((rt & 0x80) && !(cdr.Ctrl & 0x1) && cdr.Readed == 0) {
		cdr.Readed = 1;
		cdr.pTransfer = cdr.Transfer;

		switch (cdr.Mode&0x30) {
			case 0x10:
			case 0x00: cdr.pTransfer+=12; break;
			default: break;
		}
	}
}

void psxDma3(u32 madr, u32 bcr, u32 chcr) {
	u32 cdsize;
	int size;
	u8 *ptr;

#ifdef CDR_LOG
	CDR_LOG("psxDma3() Log: *** DMA 3 *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif

	switch (chcr) {
		case 0x11000000:
		case 0x11400100:
			if (cdr.Readed == 0) {
#ifdef CDR_LOG
				CDR_LOG("psxDma3() Log: *** DMA 3 *** NOT READY\n");
#endif
				break;
			}

			cdsize = (bcr & 0xffff) << 2;

			// Ape Escape: bcr = 0001 / 0000
			// - fix boot
			if( cdsize == 0 )
			{
				switch (cdr.Mode & (MODE_SIZE_2340|MODE_SIZE_2328)) {
					case MODE_SIZE_2340: cdsize = 2340; break;
					case MODE_SIZE_2328: cdsize = 2328; break;
					default:
					case MODE_SIZE_2048: cdsize = 2048; break;
				}
			}


			ptr = (u8 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CDR_LOG("psxDma3() Log: *** DMA 3 *** NULL Pointer!\n");
#endif
				break;
			}

			/*
			GS CDX: Enhancement CD crash
			- Setloc 0:0:0
			- CdlPlay
			- Spams DMA3 and gets buffer overrun
			*/
			size = CD_FRAMESIZE_RAW - (cdr.pTransfer - cdr.Transfer);
			if (size > cdsize)
				size = cdsize;
			if (size > 0)
			{
				//memcpy(ptr, pTransfer, size);
				cacheable_kernel_memcpy(ptr, pTransfer, size);
			}

			psxCpu->Clear(madr, cdsize >> 2);
			cdr.pTransfer += cdsize;

			if( chcr == 0x11400100 ) {
				HW_DMA3_MADR = SWAPu32(madr + cdsize);
				CDRDMA_INT( cdsize >> 5 );
			}
			else if( chcr == 0x11000000 ) {
				// CDRDMA_INT( (cdsize/4) * 1 );
				// halted
				psxRegs.cycle += (((cdsize >> 2) * 24 ) >> 1);
				CDRDMA_INT(16);
			}
			return;

		default:
#ifdef CDR_LOG
			CDR_LOG("psxDma3() Log: Unknown cddma %lx\n", chcr);
#endif
			break;
	}

	HW_DMA3_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(3);
}

void cdrDmaInterrupt()
{
	if (HW_DMA3_CHCR & SWAP32(0x01000000))
	{
		HW_DMA3_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(3);
	}
}

static void getCdInfo(void)
{
	u8 tmp;

	CDR_getTN(cdr.ResultTN);
	CDR_getTD(0, cdr.SetSectorEnd);
	tmp = cdr.SetSectorEnd[0];
	cdr.SetSectorEnd[0] = cdr.SetSectorEnd[2];
	cdr.SetSectorEnd[2] = tmp;
}

void cdrReset() {
	memset(&cdr, 0, sizeof(cdr));
	cdr.CurTrack = 1;
	cdr.File = 1;
	cdr.Channel = 1;
	cdr.Reg2 = 0x1f;
	cdr.Stat = NoIntr;
	cdr.DriveState = DRIVESTATE_STANDBY;
	cdr.StatP = STATUS_ROTATING;
	cdr.pTransfer = cdr.Transfer;
	getCdInfo();
}

int cdrFreeze(gzFile f, int Mode) {
	uintptr_t tmp;

	gzfreeze(&cdr, sizeof(cdr));

	if (Mode == 1) tmp = cdr.pTransfer - cdr.Transfer;
	gzfreezel(&tmp);
	if (Mode == 0) cdr.pTransfer = cdr.Transfer + tmp;

	return 0;
}

void LidInterrupt() {
	getCdInfo();
	StopCdda();

    cdr.StatP |= STATUS_SHELLOPEN;
    cdr.DriveState = DRIVESTATE_RESCAN_CD;

	cdrLidSeekInterrupt();
}
