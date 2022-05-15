#ifndef __FP_ID_H
#define __FP_ID_H

#define SUCCESSFP 0
#define ERRORFP   1

#define FPC_BASE				0x00000100
#define FPC_FPC1022				(FPC_BASE + 1)
#define FPC_FPC1245				(FPC_BASE + 2)
#define FPC_FPC1511				(FPC_BASE + 3)

#define GOODIX_BASE				0x00000200
#define GOODIX_GF3118M			(GOODIX_BASE + 1)
#define GOODIX_GF5116M			(GOODIX_BASE + 2)
#define GOODIX_GF52X6			(GOODIX_BASE + 3)
#define GOODIX_GF3208			(GOODIX_BASE + 4)
#define GOODIX_GF5269			(GOODIX_BASE + 5)
#define GOODIX_GF5288			(GOODIX_BASE + 6)
#define GOODIX_GF9118			(GOODIX_BASE + 7)
#define GOODIX_GF9518			(GOODIX_BASE + 8)
#define GOODIX_GF9518N			(GOODIX_BASE + 9)
#define GOODIX_GF3658			(GOODIX_BASE + 10)
#define GOODIX_GF9608			(GOODIX_BASE + 11)
#define GOODIX_GF3626			(GOODIX_BASE + 12)

#define OXI_BASE				0x00000300
#define OXI_OX2102			    (OXI_BASE + 1)

#define EGIS_BASE				0x00000400
#define EGIS_ET713				(EGIS_BASE + 1)

#define FPC_FRAME_ID			1
#define GOODIX_FRAME_ID			2


int get_fp_id(void);
uint8_t is_support_soft_fpid(void);

#endif

