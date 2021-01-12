/* General Purpose Registers (GPRs) */

#define    r0    0
#define    r1    1
#define    r2    2
#define    r3    3
#define    r4    4
#define    r5    5
#define    r6    6
#define    r7    7
#define    r8    8
#define    r9    9
#define    r10    10
#define    r11    11
#define    r12    12
#define    r13    13
#define    r14    14
#define    r15    15
#define    r16    16
#define    r17    17
#define    r18    18
#define    r19    19
#define    r20    20
#define    r21    21
#define    r22    22
#define    r23    23
#define    r24    24
#define    r25    25
#define    r26    26
#define    r27    27
#define    r28    28
#define    r29    29
#define    r30    30
#define    r31    31

/* Floating Point Registers (FPRs) */

#define	fr0		0
#define	fr1		1
#define	fr2		2
#define	fr3		3
#define	fr4		4
#define	fr5		5
#define	fr6		6
#define	fr7		7
#define	fr8		8
#define	fr9		9
#define	fr10	10
#define	fr11	11
#define	fr12	12
#define	fr13	13
#define	fr14	14
#define	fr15	15
#define	fr16	16
#define	fr17	17
#define	fr18	18
#define	fr19	19
#define	fr20	20
#define	fr21	21
#define	fr22	22
#define	fr23	23
#define	fr24	24
#define	fr25	25
#define	fr26	26
#define	fr27	27
#define	fr28	28
#define	fr29	29
#define	fr30	30
#define	fr31	31

	.globl myps_guVecMultiply
	// r3 = mt, r4 = src, r5 = dst
myps_guVecMultiply:
	psq_l		fr0,0(r4),0,0        // fr0 = x y
	psq_l		fr2,0(r3),0,0        // fr2 = m[0][0], m[0][1]
	psq_l		fr1,8(r4),1,0        // fr1 = z
	ps_mul		fr4,fr2,fr0          // fr4 = m[0][0] * x, m[0][1] * y
	psq_l		fr3,8(r3),1,0        // fr3 = m[0][2]
	ps_madd		fr5,fr3,fr1,fr4      // fr5 = (m[0][2] * z) + (m[0][0] * x), (0 * 0) + (m[0][1] * y)
    psq_l		fr8,12(r3),0,0       // fr8 = m[1][0], m[1][1]
	ps_sum0		fr6,fr5,fr6,fr5      // fr6 = (m[0][2] * z) + (m[0][0] * x) + (0 * 0) + (m[0][1] * y), 0
	psq_l		fr9,20(r3),1,0       // fr9 = m[1][2]
	ps_mul		fr10,fr8,fr0         // fr10 = m[1][0] * x, m[1][1] * y
	psq_st		fr6,0(r5),1,0        // r5 = result_X
	ps_madd		fr11,fr9,fr1,fr10    // fr11 = (m[1][2] * z) + (m[1][0] * x), (0 * 0) + (m[1][1] * y)
	psq_l		fr2,24(r3),0,0       // fr2 = m[2][0], m[2][1]
	ps_sum0		fr12,fr11,fr12,fr11  // fr12 = (m[1][2] * z) + (m[1][0] * x) + (0 * 0) + (m[1][1] * y), 0 ?
	psq_l		fr3,32(r3),1,0       // fr3 = m[2][2]
	ps_mul		fr4,fr2,fr0
	psq_st		fr12,4(r5),1,0       // r5 = result_Y
	ps_madd		fr5,fr3,fr1,fr4
	ps_sum0		fr6,fr5,fr6,fr5
	psq_st		fr6,8(r5),1,0        // r5 = result_Z
	blr

	.globl ps_gte_rtps
	// r3 = mt, r4 = src, r5 = add, r6 = dst, r7 = div(1.0f,1.0f or 0.000244f, 0.000244f)
ps_gte_rtps:
	psq_l		fr0,0(r4),0,0        // fr0 = x y
	psq_l		fr2,0(r3),0,0        // fr2 = m[0][0], m[0][1]
	psq_l		fr1,8(r4),1,0        // fr1 = z
	ps_mul		fr4,fr2,fr0          // fr4 = m[0][0] * x, m[0][1] * y
	psq_l		fr3,8(r3),1,0        // fr3 = m[0][2]
	ps_madd		fr5,fr3,fr1,fr4      // fr5 = (m[0][2] * z) + (m[0][0] * x), (0 * 0) + (m[0][1] * y)
    psq_l		fr8,12(r3),0,0       // fr8 = m[1][0], m[1][1]
	ps_sum0		fr6,fr5,fr6,fr5      // fr6 = (m[0][2] * z) + (m[0][0] * x) + (0 * 0) + (m[0][1] * y), 0
	psq_l		fr9,20(r3),1,0       // fr9 = m[1][2]
	ps_mul		fr10,fr8,fr0         // fr10 = m[1][0] * x, m[1][1] * y
	//psq_st		fr6,0(r5),1,0        // r5 = result_X
	ps_madd		fr11,fr9,fr1,fr10    // fr11 = (m[1][2] * z) + (m[1][0] * x), (0 * 0) + (m[1][1] * y)
	psq_l		fr2,24(r3),0,0       // fr2 = m[2][0], m[2][1]
	ps_sum0		fr12,fr11,fr12,fr11  // fr12 = (m[1][2] * z) + (m[1][0] * x) + (0 * 0) + (m[1][1] * y), 0
	psq_l		fr3,32(r3),1,0       // fr3 = m[2][2]
	ps_mul		fr4,fr2,fr0
	//psq_st		fr12,4(r5),1,0       // r5 = result_Y

    psq_l		fr7,0(r7),0,0        // fr7 = 1.0f,1.0f or 0.000244f, 0.000244f

	ps_merge00  fr6,fr6,fr12         // fr6 = result_X, result_Y
	psq_l		fr0,0(r5),0,0        // fr0 = addX addY
	ps_madd		fr8,fr6,fr7,fr0      // fr8 = result_X / 4096.0 + addX, result_Y / 4096.0 + addY

	ps_madd		fr5,fr3,fr1,fr4
	psq_l		fr9,8(r5),1,0        // fr9 = addZ
	ps_sum0		fr6,fr5,fr6,fr5
	//psq_st		fr6,8(r5),1,0        // r5 = result_Z

	lfs		    fr11,FncOverflowH@sdarel(r13)
	lfs		    fr12,FncOverflowL@sdarel(r13)
	ps_madd		fr6,fr6,fr7,fr9      // fr6 = result_Z / 4096.0 + addZ, 0

OverflowLChkX:
    ps_cmpo0    fr0,fr8,fr12         // if ((result_X / 4096.0 + addX) < -2147483648.0)
    andi.       r7,r7,0
    blt         OverflowLFlagX
OverflowHChkX:
    ps_cmpo0    fr0,fr11,fr8         // if (2147483647 < (result_X / 4096.0 + addX))
    blt         OverflowHFlagX
OverflowLChkY:
    ps_cmpo1    fr0,fr8,fr12         // if ((result_Y / 4096.0 + addY) < -2147483648.0)
    blt         OverflowLFlagY
OverflowHChkY:
    ps_cmpo1    fr0,fr11,fr8         // if (2147483647 < (result_Y / 4096.0 + addY))
    blt         OverflowHFlagY
OverflowLChkZ:
    ps_cmpo0    fr0,fr6,fr12         // if ((result_Z / 4096.0 + addZ) < -2147483648.0)
    blt         OverflowLFlagZ
OverflowHChkZ:
    ps_cmpo0    fr0,fr11,fr6         // if (2147483647 < (result_Z / 4096.0 + addZ))
    blt         OverflowHFlagZ
    b           SetReturnVal
OverflowLFlagX:
    oris        r7,r7,0x2000         // gteFLAG |= 1<<29;
    b           OverflowHChkX
OverflowHFlagX:
    oris        r7,r7,0x400          // gteFLAG |= 1<<26;
    b           OverflowLChkY
OverflowLFlagY:
    oris        r7,r7,0x1000         // gteFLAG |= 1<<28;
    b           OverflowHChkY
OverflowHFlagY:
    oris        r7,r7,0x200          // gteFLAG |= 1<<25;
    b           OverflowLChkZ
OverflowLFlagZ:
    oris        r7,r7,0x800          // gteFLAG |= 1<<27;
    b           OverflowHChkZ
OverflowHFlagZ:
    oris        r7,r7,0x100          // gteFLAG |= 1<<24;
SetReturnVal:
    psq_st		fr8,0(r6),0,0        // r6 = X, Y
    psq_st		fr6,8(r6),1,0        // r6 = X, Y, Z
    stw         r7,12(r6)            // r6 = X, Y, Z, gteFlag
	blr

	.globl ps_gte_DoubleTrans
	// r3 = Mtx6X3, r4 = src, r5 = add, r6 = dst, r7 = div(0.000244f, 0.000244f)
ps_gte_DoubleTrans:
    andi.       r9,r9,0
    andi.       r10,r10,0
    lfs		    fr15,Unit0@sdarel(r13)
	lfs		    fr16,Unit0@sdarel(r13)
	ps_merge00  fr15,fr15,fr16       // fr15 = 0.0, 0.0
	b           startTrans
nextTrans:
    addic       r10,r10,1
    psq_l		fr15,0(r5),0,0       // fr15 = addX, addY
    psq_l		fr16,8(r5),1,0       // fr16 = addZ
    addic       r3,r3,36
    psq_st		fr8,0(r4),0,0        // r4 = newX, newY
    psq_st		fr6,8(r4),1,0        // r4 = newX, newY, newZ
startTrans:
	psq_l		fr0,0(r4),0,0        // fr0 = x y
	psq_l		fr2,0(r3),0,0        // fr2 = m[0][0], m[0][1]
	psq_l		fr1,8(r4),1,0        // fr1 = z
	ps_mul		fr4,fr2,fr0          // fr4 = m[0][0] * x, m[0][1] * y
	psq_l		fr3,8(r3),1,0        // fr3 = m[0][2]
	ps_madd		fr5,fr3,fr1,fr4      // fr5 = (m[0][2] * z) + (m[0][0] * x), (0 * 0) + (m[0][1] * y)
    psq_l		fr8,12(r3),0,0       // fr8 = m[1][0], m[1][1]
	ps_sum0		fr6,fr5,fr6,fr5      // fr6 = (m[0][2] * z) + (m[0][0] * x) + (0 * 0) + (m[0][1] * y), 0
	psq_l		fr9,20(r3),1,0       // fr9 = m[1][2]
	ps_mul		fr10,fr8,fr0         // fr10 = m[1][0] * x, m[1][1] * y
	//psq_st		fr6,0(r5),1,0        // r5 = result_X
	ps_madd		fr11,fr9,fr1,fr10    // fr11 = (m[1][2] * z) + (m[1][0] * x), (0 * 0) + (m[1][1] * y)
	psq_l		fr2,24(r3),0,0       // fr2 = m[2][0], m[2][1]
	ps_sum0		fr12,fr11,fr12,fr11  // fr12 = (m[1][2] * z) + (m[1][0] * x) + (0 * 0) + (m[1][1] * y), 0
	psq_l		fr3,32(r3),1,0       // fr3 = m[2][2]
	ps_mul		fr4,fr2,fr0
	//psq_st		fr12,4(r5),1,0       // r5 = result_Y

    psq_l		fr7,0(r7),0,0        // fr7 = 1.0f,1.0f or 0.000244f, 0.000244f

	ps_merge00  fr6,fr6,fr12         // fr6 = result_X, result_Y
	ps_madd		fr8,fr6,fr7,fr15     // fr8 = result_X / 4096.0 + addX, result_Y / 4096.0 + addY

	ps_madd		fr5,fr3,fr1,fr4
	ps_sum0		fr6,fr5,fr6,fr5
	//psq_st		fr6,8(r5),1,0        // r5 = result_Z

	lfs		    fr11,F12limA1UOverFlowH@sdarel(r13)
	lfs		    fr12,F12limA1UOverFlowL@sdarel(r13)
	ps_madd		fr6,fr6,fr7,fr16      // fr6 = result_Z / 4096.0 + addZ

F12OverflowLChkX:
    ps_cmpo0    fr0,fr8,fr12         // if ((result_X / 4096.0) < 0.0)
    blt         F12OverflowFlagX
F12OverflowHChkX:
    ps_cmpo0    fr0,fr11,fr8         // if ((32767 << 12) < (result_X / 4096.0))
    blt         F12OverflowFlagX
F12OverflowLChkY:
    ps_cmpo1    fr0,fr8,fr12         // if ((result_Y / 4096.0) < 0.0)
    blt         F12OverflowFlagY
F12OverflowHChkY:
    ps_cmpo1    fr0,fr11,fr8         // if ((32767 << 12) < (result_Y / 4096.0))
    blt         F12OverflowFlagY
F12OverflowLChkZ:
    ps_cmpo0    fr0,fr6,fr12         // if ((result_Z / 4096.0) < 0.0)
    blt         F12OverflowFlagZ
F12OverflowHChkZ:
    ps_cmpo0    fr0,fr11,fr6         // if ((32767 << 12) < (result_Z / 4096.0))
    blt         F12OverflowFlagZ
    b           CheckLoop

F12OverflowFlagX:
    oris        r9,r9,0x100          // gteFLAG |= 1<<24;
    b           F12OverflowLChkY
F12OverflowFlagY:
    oris        r9,r9,0x80           // gteFLAG |= 1<<23;
    b           F12OverflowLChkZ
F12OverflowFlagZ:
    oris        r9,r9,0x40           // gteFLAG |= 1<<22;

CheckLoop:
    cmpi        r0,0,r10,1
    blt         nextTrans

    psq_st		fr8,0(r6),0,0        // r6 = X, Y
    psq_st		fr6,8(r6),1,0        // r6 = X, Y, Z
    stw         r9,12(r6)            // r6 = X, Y, Z, gteFlag
	blr


	.section .sdata
	.balign 32
Unit0:
	.float	0.0
Unit1:
	.float	1.0, 1.0
Unit4096:
	.float	0.000244, 0.000244       // 1 >> 12 = 1 / 4096 = 0.000244140625
FncOverflowH:
	.float	2147483647.0
FncOverflowL:
	.float	-2147483648.0
F12limA1UOverFlowH:
	.float	134213632.0              // 32767 << 12 = 134213632
F12limA1UOverFlowL:
	.float	0.0

