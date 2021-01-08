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
	psq_l		fr1,8(r4),1,0        // fr1 = z 0
	ps_mul		fr4,fr2,fr0          // fr4 = m[0][0] * x, m[0][1] * y
	psq_l		fr3,8(r3),1,0        // fr3 = m[0][2], m[0][3] --------------------
	ps_madd		fr5,fr3,fr1,fr4      // fr5 = (m[0][2] * z) + (m[0][0] * x), (m[0][3] * 1.0) + (m[0][1] * y)
    psq_l		fr8,12(r3),0,0       // fr8 = m[1][0], m[1][1] ====================
	ps_sum0		fr6,fr5,fr6,fr5      // fr6 = (m[0][2] * z) + (m[0][0] * x) + (m[0][3] * 1.0) + (m[0][1] * y), 0 ?
	psq_l		fr9,20(r3),1,0       // fr9 = m[1][2], m[1][3] --------------------
	ps_mul		fr10,fr8,fr0         // fr10 = m[1][0] * x, m[1][1] * y
	psq_st		fr6,0(r5),1,0        // r5 = result_X
	ps_madd		fr11,fr9,fr1,fr10    // fr11 = (m[1][2] * z) + (m[1][0] * x), (m[1][3] * 1.0) + (m[1][1] * y)
	psq_l		fr2,24(r3),0,0       // fr2 = m[2][0], m[2][1] =======================
	ps_sum0		fr12,fr11,fr12,fr11  // fr12 = (m[1][2] * z) + (m[1][0] * x) + (m[1][3] * 1.0) + (m[1][1] * y), 0 ?
	psq_l		fr3,32(r3),1,0       // fr3 = m[2][2], m[2][3] ---------------------
	ps_mul		fr4,fr2,fr0
	psq_st		fr12,4(r5),1,0       // r5 = result_Y
	ps_madd		fr5,fr3,fr1,fr4
	ps_sum0		fr6,fr5,fr6,fr5
	psq_st		fr6,8(r5),1,0        // r5 = result_Z
	blr

	.globl myps_guVecMultiplySR
	// r3 = mt, r4 = src, r5 = dst
myps_guVecMultiplySR:
    psq_l		fr0,0(r3),0,0    // m[0][0], m[0][1] GQR0 = 0
    psq_l		fr6,0(r4),0,0    // fp6 - x y
    psq_l		fr2,16(r3),0,0   // m[1][0], m[1][1]
    ps_mul		fr8,fr0,fr6      // fp8 = m00x m01y // next X
    psq_l		fr4,32(r3),0,0   // m[2][0], m[2][1]
    ps_mul		fr10,fr2,fr6     // fp10 = m10x m11y // next Y
    psq_l		fr7,8(r4),1,0    // fp7 - z,1.0
    // fp12 = m20x m21y // next Z
    ps_mul		fr12,fr4,fr6  // YYY last FP6 usage
    psq_l		fr3,24(r3),0,0   // m[1][2], m[1][3]
    ps_sum0		fr8,fr8,fr8,fr8
    psq_l		fr5,40(r3),0,0   // m[2][2], m[2][3]
    ps_sum0		fr10,fr10,fr10,fr10
    psq_l		fr1,8(r3),0,0    // m[0][2], m[0][3]
    ps_sum0		fr12,fr12,fr12,fr12
    ps_madd		fr9,fr1,fr7,fr8
    psq_st		fr9,0(r5),1,0      // store X
    ps_madd		fr11,fr3,fr7,fr10
    psq_st		fr11,4(r5),1,0      // store Y
    ps_madd		fr13,fr5,fr7,fr12
    psq_st		fr13,8(r5),1,0      //  sore Z
	blr

	.section .sdata
	.balign 16
Unit01:
	.float	0.0, 1.0
QuatEpsilon:
	.float	0.00001
NrmData:
	.float	0.5, 3.0
