/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/clock.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/mx6ul_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/io.h>
#include <common.h>
#include <fsl_esdhc.h>
#include <linux/sizes.h>
#include <linux/fb.h>
#include <miiphy.h>
#include <mmc.h>
#include <mxsfb.h>
#include <netdev.h>
#include <usb.h>
#include <usb/ehci-ci.h>
#include <stdlib.h>

/* Temporary include config header for correct Eclipse indexes */
#include <configs/mx6ul_sonar_ip_control.h>

#ifdef CONFIG_POWER
#include <power/pmic.h>
#include <power/pfuze300_pmic.h>
#include "../common/pfuze.h"
#endif

#ifdef CONFIG_FASTBOOT
#include <fastboot.h>
#ifdef CONFIG_ANDROID_RECOVERY
#include <recovery.h>
#endif
#endif /*CONFIG_FASTBOOT*/


DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_22K_UP  | PAD_CTL_SPEED_LOW |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_DAT3_CD_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_DOWN  | PAD_CTL_SPEED_LOW |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)


#define ENET_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_SPEED_HIGH   |                                   \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST)

#define MDIO_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST | PAD_CTL_ODE)


#define ENET_CLK_PAD_CTRL  (PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST)

#define ENET_RX_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |          \
	PAD_CTL_SPEED_HIGH   | PAD_CTL_SRE_FAST)

#define LCD_PAD_CTRL    (PAD_CTL_HYS | PAD_CTL_PUS_100K_UP | PAD_CTL_PUE | \
	PAD_CTL_PKE | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm)

#define GPMI_PAD_CTRL0 (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define GPMI_PAD_CTRL1 (PAD_CTL_DSE_40ohm | PAD_CTL_SPEED_MED | \
			PAD_CTL_SRE_FAST)
#define GPMI_PAD_CTRL2 (GPMI_PAD_CTRL0 | GPMI_PAD_CTRL1)

#define WEIM_NOR_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE | \
		PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | \
		PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST)

#define SPI_PAD_CTRL (PAD_CTL_HYS |				\
	PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST)

#define OTG_ID_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_LOW |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define IOX_SDI IMX_GPIO_NR(5, 10)
#define IOX_STCP IMX_GPIO_NR(5, 7)
#define IOX_SHCP IMX_GPIO_NR(5, 11)
#define IOX_OE IMX_GPIO_NR(5, 18)

static iomux_v3_cfg_t const iox_pads[] = {
	/* IOX_SDI */
	MX6_PAD_BOOT_MODE0__GPIO5_IO10 | MUX_PAD_CTRL(NO_PAD_CTRL),
	/* IOX_SHCP */
	MX6_PAD_BOOT_MODE1__GPIO5_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL),
	/* IOX_STCP */
	MX6_PAD_SNVS_TAMPER7__GPIO5_IO07 | MUX_PAD_CTRL(NO_PAD_CTRL),
	/* IOX_nOE */
	MX6_PAD_SNVS_TAMPER8__GPIO5_IO08 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

/*
 * HDMI_nRST --> Q0
 * ENET1_nRST --> Q1
 * ENET2_nRST --> Q2
 * CAN1_2_STBY --> Q3
 * BT_nPWD --> Q4
 * CSI_RST --> Q5
 * CSI_PWDN --> Q6
 * LCD_nPWREN --> Q7
 */
enum qn {
	HDMI_nRST,
	ENET1_nRST,
	ENET2_nRST,
	CAN1_2_STBY,
	BT_nPWD,
	CSI_RST,
	CSI_PWDN,
	LCD_nPWREN,
};

enum qn_func {
	qn_reset,
	qn_enable,
	qn_disable,
};

enum qn_level {
	qn_low = 0,
	qn_high = 1,
};

static enum qn_level seq[3][2] = {
	{0, 1}, {1, 1}, {0, 0}
};

static enum qn_func qn_output[8] = {
	qn_reset, qn_reset, qn_reset, qn_enable, qn_disable, qn_reset, qn_disable,
	qn_enable
};

typedef struct {
	char *kernel_address;
	char *dtb_address;
	char *rootfs_mtd_number;
} fw_envs_t;

fw_envs_t fw_envs[2] = {
		{.kernel_address = CONFIG_KERNEL_ADDR_FW1,
		.dtb_address = CONFIG_DTB_ADDR_FW1,
		.rootfs_mtd_number = CONFIG_ROOTFS_MTD_PART_FW1},

		{.kernel_address = CONFIG_KERNEL_ADDR_FW2,
		.dtb_address = CONFIG_DTB_ADDR_FW2,
		.rootfs_mtd_number = CONFIG_ROOTFS_MTD_PART_FW2}
};

//return boot count, in error return -1
int get_boot_count(void)
{
	char* boot_count_ch = getenv(CONFIG_BOOT_COUNT_VAR_NAME);

	if (boot_count_ch == NULL)
		return -1;

	int boot_count = boot_count_ch[0] - '0';
	printf("Boot count: %d\n", boot_count);

	return boot_count;
}

// return 0 - variable seted succesfully, >0 - error
int set_boot_count(ulong boot_count)
{
	if (boot_count > (CONFIG_MAX_BOOT_COUNT[0] - '0'))
		return 1;

	char *boot_count_ch = simple_itoa(boot_count);

	printf("Set boot count: %u\n", (uint)boot_count);

	if (setenv(CONFIG_BOOT_COUNT_VAR_NAME, boot_count_ch) != 0)
		return 1;

	return saveenv();
}

//decrement boot count and return new value, in error return -1
int dec_boot_count()
{
	int boot_count = get_boot_count();

	if (boot_count < 0) {
		return -1;
	} else if (boot_count > 0) {
		boot_count--;
		if (set_boot_count((ulong)boot_count) != 0)
			return -1;
	}

	return boot_count;
}

/*set boot count in max value
 * return 0 - ok, >0 - error
 */
int reset_boot_count(void)
{
	ulong boot_count = CONFIG_MAX_BOOT_COUNT[0] - '0';

	return set_boot_count(boot_count);
}

//if return <0 - error
int get_fw_number(void)
{
	char *fw_number_ch = getenv(CONFIG_FW_NUMBER_VAR_NAME);

	if (fw_number_ch == NULL)
		return -1;

	int curr_fw_number = fw_number_ch[0] - '0';

	printf("Current firmware number: %d\n", curr_fw_number);

	return curr_fw_number;
}

/*
 * set fw_number variable
 * return 0 - ok, >0 - error
 */
int set_fw_number(uint fw_number)
{
	if (fw_number < 1 || fw_number > 2)
		return 1;

	char *fw_number_ch = simple_itoa(fw_number);

	printf("Set firmware number: %d\n", fw_number);

	if (setenv(CONFIG_FW_NUMBER_VAR_NAME, fw_number_ch) != 0)
		return 1;

	return saveenv();
}

/* set envs for boot fw
 * return 0 - ok, >0 - error
 */
int set_fw_envs(uint fw_number)
{
	if (setenv(CONFIG_KERNEL_ADDR_VAR_NAME, fw_envs[fw_number - 1].kernel_address) != 0)
		return 1;

	if (setenv(CONFIG_DTB_ADDR_VAR_NAME, fw_envs[fw_number - 1].dtb_address) != 0) {
		setenv(CONFIG_KERNEL_ADDR_VAR_NAME, fw_envs[!(fw_number - 1)].kernel_address);
		return 1;
	}

	char* boot_args = CONFIG_BOOTARGS_TEMPLATE;
	sprintf(boot_args, CONFIG_BOOTARGS_TEMPLATE, fw_envs[fw_number - 1].rootfs_mtd_number);

	if (setenv(CONFIG_BOOTARGS_VAR_NAME, boot_args) != 0) {
		setenv(CONFIG_KERNEL_ADDR_VAR_NAME, fw_envs[!(fw_number - 1)].kernel_address);
		setenv(CONFIG_DTB_ADDR_VAR_NAME, fw_envs[!(fw_number - 1)].dtb_address);
		return 1;
	}

	return saveenv();
}

/* switch to new firmware
 * return 0 - ok, >0 - error
 */
int fw_switch(void)
{
	int curr_fw_number = get_fw_number();

	if (curr_fw_number < 0)
		return 1;

	uint new_fw_number = (uint)(!(curr_fw_number - 1) + 1);

	if (set_fw_number(new_fw_number) > 0)
		return 1;

	if (reset_boot_count() > 0)
		return 1;

	printf("Switch to new firmware %u\n", new_fw_number);

	return set_fw_envs(new_fw_number);
}

void iox74lv_init(void)
{
	int i;

	gpio_direction_output(IOX_OE, 0);

	for (i = 7; i >= 0; i--) {
		gpio_direction_output(IOX_SHCP, 0);
		gpio_direction_output(IOX_SDI, seq[qn_output[i]][0]);
		udelay(500);
		gpio_direction_output(IOX_SHCP, 1);
		udelay(500);
	}

	gpio_direction_output(IOX_STCP, 0);
	udelay(500);
	/*
	  * shift register will be output to pins
	  */
	gpio_direction_output(IOX_STCP, 1);

	for (i = 7; i >= 0; i--) {
		gpio_direction_output(IOX_SHCP, 0);
		gpio_direction_output(IOX_SDI, seq[qn_output[i]][1]);
		udelay(500);
		gpio_direction_output(IOX_SHCP, 1);
		udelay(500);
	}
	gpio_direction_output(IOX_STCP, 0);
	udelay(500);
	/*
	  * shift register will be output to pins
	  */
	gpio_direction_output(IOX_STCP, 1);

	gpio_direction_output(IOX_OE, 1);
};

void iox74lv_set(int index)
{
	int i;

	gpio_direction_output(IOX_OE, 0);

	for (i = 7; i >= 0; i--) {
		gpio_direction_output(IOX_SHCP, 0);

		if (i == index)
			gpio_direction_output(IOX_SDI, seq[qn_output[i]][0]);
		else
			gpio_direction_output(IOX_SDI, seq[qn_output[i]][1]);
		udelay(500);
		gpio_direction_output(IOX_SHCP, 1);
		udelay(500);
	}

	gpio_direction_output(IOX_STCP, 0);
	udelay(500);
	/*
	  * shift register will be output to pins
	  */
	gpio_direction_output(IOX_STCP, 1);

	for (i = 7; i >= 0; i--) {
		gpio_direction_output(IOX_SHCP, 0);
		gpio_direction_output(IOX_SDI, seq[qn_output[i]][1]);
		udelay(500);
		gpio_direction_output(IOX_SHCP, 1);
		udelay(500);
	}

	gpio_direction_output(IOX_STCP, 0);
	udelay(500);
	/*
	  * shift register will be output to pins
	  */
	gpio_direction_output(IOX_STCP, 1);

	gpio_direction_output(IOX_OE, 1);
};



int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_SIZE;

	return 0;
}

static iomux_v3_cfg_t const uart1_pads[] = {
	MX6_PAD_UART1_TX_DATA__UART1_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_UART1_RX_DATA__UART1_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc1_pads[] = {
	MX6_PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),

	/* VSELECT */
	MX6_PAD_GPIO1_IO05__USDHC1_VSELECT | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	/* CD */
	MX6_PAD_UART1_RTS_B__GPIO1_IO19 | MUX_PAD_CTRL(NO_PAD_CTRL),
	/* RST_B */
	MX6_PAD_GPIO1_IO09__GPIO1_IO09 | MUX_PAD_CTRL(NO_PAD_CTRL),
};


static iomux_v3_cfg_t const usdhc2_pads[] = {
	MX6_PAD_NAND_RE_B__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NAND_WE_B__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NAND_DATA00__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NAND_DATA01__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NAND_DATA02__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NAND_DATA03__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_cd_pads[] = {

	/* The evk board uses DAT3 to detect CD card plugin, in u-boot we mux the pin to
	  * GPIO when doing board_mmc_getcd.
	  */
	MX6_PAD_NAND_DATA03__GPIO4_IO05 | MUX_PAD_CTRL(USDHC_DAT3_CD_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_dat3_pads[] = {
	MX6_PAD_NAND_DATA03__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_DAT3_CD_PAD_CTRL),
};




#ifdef CONFIG_SYS_USE_NAND
static iomux_v3_cfg_t const nand_pads[] = {
	MX6_PAD_NAND_DATA00__RAWNAND_DATA00 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA01__RAWNAND_DATA01 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA02__RAWNAND_DATA02 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA03__RAWNAND_DATA03 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA04__RAWNAND_DATA04 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA05__RAWNAND_DATA05 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA06__RAWNAND_DATA06 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA07__RAWNAND_DATA07 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_CLE__RAWNAND_CLE | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_ALE__RAWNAND_ALE | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_CE0_B__RAWNAND_CE0_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_CE1_B__RAWNAND_CE1_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_RE_B__RAWNAND_RE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_WE_B__RAWNAND_WE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_WP_B__RAWNAND_WP_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_READY_B__RAWNAND_READY_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DQS__RAWNAND_DQS | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
};

static void setup_gpmi_nand(void)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	/* config gpmi nand iomux */
	imx_iomux_v3_setup_multiple_pads(nand_pads, ARRAY_SIZE(nand_pads));

	clrbits_le32(&mxc_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);

	/*
	 * config gpmi and bch clock to 100 MHz
	 * bch/gpmi select PLL2 PFD2 400M
	 * 100M = 400M / 4
	 */
	clrbits_le32(&mxc_ccm->cscmr1,
		     MXC_CCM_CSCMR1_BCH_CLK_SEL |
		     MXC_CCM_CSCMR1_GPMI_CLK_SEL);
	clrsetbits_le32(&mxc_ccm->cscdr1,
			MXC_CCM_CSCDR1_BCH_PODF_MASK |
			MXC_CCM_CSCDR1_GPMI_PODF_MASK,
			(3 << MXC_CCM_CSCDR1_BCH_PODF_OFFSET) |
			(3 << MXC_CCM_CSCDR1_GPMI_PODF_OFFSET));

	/* enable gpmi and bch clock gating */
	setbits_le32(&mxc_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);

	/* enable apbh clock gating */
	setbits_le32(&mxc_ccm->CCGR0, MXC_CCM_CCGR0_APBHDMA_MASK);
}
#endif

#ifdef CONFIG_FEC_MXC
/*
 * pin conflicts for fec1 and fec2, GPIO1_IO06 and GPIO1_IO07 can only
 * be used for ENET1 or ENET2, cannot be used for both.
 */
static iomux_v3_cfg_t const fec1_pads[] = {
	MX6_PAD_GPIO1_IO06__ENET1_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL),
	MX6_PAD_GPIO1_IO07__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_DATA0__ENET1_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_DATA1__ENET1_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_EN__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_CLK__ENET1_REF_CLK1 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL),
	MX6_PAD_ENET1_RX_DATA0__ENET1_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_DATA1__ENET1_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_ER__ENET1_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_EN__ENET1_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
};

static iomux_v3_cfg_t const fec2_pads[] = {
	MX6_PAD_GPIO1_IO06__ENET2_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL),
	MX6_PAD_GPIO1_IO07__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX6_PAD_ENET2_TX_DATA0__ENET2_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_TX_DATA1__ENET2_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_TX_CLK__ENET2_REF_CLK2 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL),
	MX6_PAD_ENET2_TX_EN__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX6_PAD_ENET2_RX_DATA0__ENET2_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_DATA1__ENET2_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_EN__ENET2_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_ER__ENET2_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL),
};

static void setup_iomux_fec(int fec_id)
{
	if (fec_id == 0)
		imx_iomux_v3_setup_multiple_pads(fec1_pads, ARRAY_SIZE(fec1_pads));
	else
		imx_iomux_v3_setup_multiple_pads(fec2_pads, ARRAY_SIZE(fec2_pads));
}
#endif

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart1_pads, ARRAY_SIZE(uart1_pads));
}



#ifdef CONFIG_VIDEO_MXS
static iomux_v3_cfg_t const lcd_pads[] = {
	MX6_PAD_LCD_CLK__LCDIF_CLK | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_ENABLE__LCDIF_ENABLE | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_HSYNC__LCDIF_HSYNC | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_VSYNC__LCDIF_VSYNC | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA00__LCDIF_DATA00 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA01__LCDIF_DATA01 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA02__LCDIF_DATA02 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA03__LCDIF_DATA03 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA04__LCDIF_DATA04 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA05__LCDIF_DATA05 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA06__LCDIF_DATA06 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA07__LCDIF_DATA07 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA08__LCDIF_DATA08 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA09__LCDIF_DATA09 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA10__LCDIF_DATA10 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA11__LCDIF_DATA11 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA12__LCDIF_DATA12 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA13__LCDIF_DATA13 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA14__LCDIF_DATA14 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA15__LCDIF_DATA15 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA16__LCDIF_DATA16 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA17__LCDIF_DATA17 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA18__LCDIF_DATA18 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA19__LCDIF_DATA19 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA20__LCDIF_DATA20 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA21__LCDIF_DATA21 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA22__LCDIF_DATA22 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA23__LCDIF_DATA23 | MUX_PAD_CTRL(LCD_PAD_CTRL),

	/*
	 * Use GPIO for LCD enable and led backlight.
	 */
	MX6_PAD_CSI_HSYNC__GPIO4_IO20 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI_DATA03__GPIO4_IO24 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI_DATA04__GPIO4_IO25 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI_DATA05__GPIO4_IO26 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

struct lcd_panel_info_t {
	unsigned int lcdif_base_addr;
	int depth;
	void (*enable)(struct lcd_panel_info_t const *dev);
	struct fb_videomode mode;
};

void do_enable_parallel_lcd(struct lcd_panel_info_t const *dev)
{
	enable_lcdif_clock(dev->lcdif_base_addr);

	imx_iomux_v3_setup_multiple_pads(lcd_pads, ARRAY_SIZE(lcd_pads));

#if 0
	/* enable LCD */
	gpio_direction_output(IMX_GPIO_NR(4, 20) , 1);
	gpio_direction_output(IMX_GPIO_NR(4, 24) , 1);
	gpio_direction_output(IMX_GPIO_NR(4, 25) , 1);
	gpio_direction_output(IMX_GPIO_NR(4, 26) , 1);
#endif
}

static struct lcd_panel_info_t const displays[] = {{
	.lcdif_base_addr = LCDIF1_BASE_ADDR,
	.depth = 24,
	.enable	= do_enable_parallel_lcd,
	.mode	= {
		.name			= "TFT43AB",
		.xres           = 640,
		.yres           = 480,
		.pixclock       = 25000000,
		.left_margin    = 8,
		.right_margin   = 4,
		.upper_margin   = 2,
		.lower_margin   = 4,
		.hsync_len      = 63,
		.vsync_len      = 45,
		.sync           = 0,
		.vmode          = FB_VMODE_NONINTERLACED
} } };

int board_video_skip(void)
{
	int i;
	int ret;
	char const *panel = getenv("panel");

	if (!panel) {
		panel = displays[0].mode.name;
		printf("No panel detected: default to %s\n", panel);
		i = 0;
	} else {
		for (i = 0; i < ARRAY_SIZE(displays); i++) {
			if (!strcmp(panel, displays[i].mode.name))
				break;
		}
	}
	if (i < ARRAY_SIZE(displays)) {
		ret = mxs_lcd_panel_setup(displays[i].mode, displays[i].depth,
				    displays[i].lcdif_base_addr);
		if (!ret) {
			if (displays[i].enable)
				displays[i].enable(displays+i);
			printf("Display: %s (%ux%u)\n",
			       displays[i].mode.name,
			       displays[i].mode.xres,
			       displays[i].mode.yres);
		} else
			printf("LCD %s cannot be configured: %d\n",
			       displays[i].mode.name, ret);
	} else {
		printf("unsupported panel %s\n", panel);
		return -EINVAL;
	}

	return 0;
}
#endif

#ifdef CONFIG_FEC_MXC
int board_eth_init(bd_t *bis)
{
	int ret;

	setup_iomux_fec(CONFIG_FEC_ENET_DEV);

	ret = fecmxc_initialize_multi(bis, CONFIG_FEC_ENET_DEV,
		CONFIG_FEC_MXC_PHYADDR, IMX_FEC_BASE);
	if (ret)
		printf("FEC%d MXC: %s:failed\n", CONFIG_FEC_ENET_DEV, __func__);

	return 0;
}

static int setup_fec(int fec_id)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs
		= (struct iomuxc_gpr_base_regs *) IOMUXC_GPR_BASE_ADDR;
	int ret;

	if (0 == fec_id) {
		/* Use 50M anatop loopback REF_CLK1 for ENET1, clear gpr1[13], set gpr1[17]*/
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1], IOMUX_GPR1_FEC1_MASK,
				IOMUX_GPR1_FEC1_CLOCK_MUX1_SEL_MASK);
	} else {
		/* Use 50M anatop loopback REF_CLK2 for ENET2, clear gpr1[14], set gpr1[18]*/
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1], IOMUX_GPR1_FEC2_MASK,
				IOMUX_GPR1_FEC2_CLOCK_MUX1_SEL_MASK);
	}

	ret = enable_fec_anatop_clock(fec_id, ENET_50MHZ);
	if (ret)
		return ret;

	enable_enet_clk(1);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x8190);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)

static iomux_v3_cfg_t const usb_otg_pads[] = {
	MX6_PAD_GPIO1_IO00__ANATOP_OTG1_ID | MUX_PAD_CTRL(OTG_ID_PAD_CTRL),
};

/* At default the 3v3 enables the MIC2026 for VBUS power */
static void setup_usb(void)
{
	imx_iomux_v3_setup_multiple_pads(usb_otg_pads,
					 ARRAY_SIZE(usb_otg_pads));
}

int board_usb_phy_mode(int port)
{
	if (port == 1)
		return USB_INIT_HOST;
	else
		return usb_phy_mode(port);
}

int board_ehci_hcd_init(int port)
{
	u32 *usbnc_usb_ctrl;

	if (port > 1)
		return -EINVAL;

	usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
				 port * 4);

	/* Set Power polarity */
	setbits_le32(usbnc_usb_ctrl, UCTRL_PWR_POL);

	return 0;
}
#endif

int board_early_init_f(void)
{
	setup_iomux_uart();

	return 0;
}

int board_init(void)
{
	printf("board_init\n");
	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	imx_iomux_v3_setup_multiple_pads(iox_pads, ARRAY_SIZE(iox_pads));

	iox74lv_init();


#ifdef	CONFIG_FEC_MXC
	setup_fec(CONFIG_FEC_ENET_DEV);
#endif

#ifdef CONFIG_SYS_USE_NAND
	setup_gpmi_nand();
#endif

#ifdef CONFIG_USB_EHCI_MX6
	setup_usb();
#endif

	return 0;
}

#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
	{"sd1", MAKE_CFGVAL(0x42, 0x20, 0x00, 0x00)},
	{"sd2", MAKE_CFGVAL(0x40, 0x28, 0x00, 0x00)},
	{"qspi1", MAKE_CFGVAL(0x10, 0x00, 0x00, 0x00)},
	{NULL,	 0},
};
#endif

int board_late_init(void)
{
	printf("board_late_init\n");

	int boot_count = get_boot_count();

	if (boot_count > 0) {
		int fw_number = get_fw_number();

		if (fw_number > 0) {
			if (set_fw_envs((uint)fw_number) > 0)
				printf("Error set firmware environments\n");
		} else {
			printf("Error firmware number: %d\n", fw_number);
		}
	} else if (boot_count == 0) {
		if (fw_switch() != 0)
			printf("Error switching to new firmware\n");
	}

#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif

	set_wdog_reset((struct wdog_regs *)WDOG1_BASE_ADDR);

	return 0;
}

u32 get_board_rev(void)
{
	printf("get_board_rev\n");
	return get_cpu_rev();
}

int checkboard(void)
{
	puts("Board: pimx-6ul\n");
	return 0;
}

#ifdef CONFIG_FASTBOOT

void board_fastboot_setup(void)
{
	switch (get_boot_device()) {
#if defined(CONFIG_FASTBOOT_STORAGE_MMC)
	case SD1_BOOT:
	case MMC1_BOOT:
		if (!getenv("fastboot_dev"))
			setenv("fastboot_dev", "mmc0");
		if (!getenv("bootcmd"))
			setenv("bootcmd", "booti mmc0");
		break;
	case SD2_BOOT:
	case MMC2_BOOT:
		if (!getenv("fastboot_dev"))
			setenv("fastboot_dev", "mmc1");
		if (!getenv("bootcmd"))
			setenv("bootcmd", "booti mmc1");
		break;
#endif /*CONFIG_FASTBOOT_STORAGE_MMC*/
#if defined(CONFIG_FASTBOOT_STORAGE_NAND)
	case NAND_BOOT:
		if (!getenv("fastboot_dev"))
			setenv("fastboot_dev", "nand");
		if (!getenv("fbparts"))
			setenv("fbparts", ANDROID_FASTBOOT_NAND_PARTS);
		if (!getenv("bootcmd"))
			setenv("bootcmd",
				"nand read ${loadaddr} ${boot_nand_offset} "
				"${boot_nand_size};booti ${loadaddr}");
		break;
#endif /*CONFIG_FASTBOOT_STORAGE_NAND*/

	default:
		printf("unsupported boot devices\n");
		break;
	}
}

#ifdef CONFIG_ANDROID_RECOVERY
int check_recovery_cmd_file(void)
{
	int recovery_mode = 0;

	recovery_mode = recovery_check_and_clean_flag();

	return recovery_mode;
}

void board_recovery_setup(void)
{
	int bootdev = get_boot_device();

	switch (bootdev) {
#if defined(CONFIG_FASTBOOT_STORAGE_MMC)
	case SD1_BOOT:
	case MMC1_BOOT:
		if (!getenv("bootcmd_android_recovery"))
			setenv("bootcmd_android_recovery", "booti mmc0 recovery");
		break;
	case SD2_BOOT:
	case MMC2_BOOT:
		if (!getenv("bootcmd_android_recovery"))
			setenv("bootcmd_android_recovery", "booti mmc1 recovery");
		break;
#endif /*CONFIG_FASTBOOT_STORAGE_MMC*/
#if defined(CONFIG_FASTBOOT_STORAGE_NAND)
	case NAND_BOOT:
		if (!getenv("bootcmd_android_recovery"))
			setenv("bootcmd_android_recovery",
				"nand read ${loadaddr} ${recovery_nand_offset} "
				"${recovery_nand_size};booti ${loadaddr}");
		break;
#endif /*CONFIG_FASTBOOT_STORAGE_NAND*/

	default:
		printf("Unsupported bootup device for recovery: dev: %d\n",
			bootdev);
		return;
	}

	printf("setup env for recovery..\n");
	setenv("bootcmd", "run bootcmd_android_recovery");
}
#endif /*CONFIG_ANDROID_RECOVERY*/

#endif /*CONFIG_FASTBOOT*/

#ifdef CONFIG_IMX_UDC
static iomux_v3_cfg_t const otg_udc_pads[] = {
	MX6_PAD_GPIO1_IO00__ANATOP_OTG1_ID | MUX_PAD_CTRL(NO_PAD_CTRL),
};

void udc_pins_setting(void)
{
	imx_iomux_v3_setup_multiple_pads(otg_udc_pads,
		ARRAY_SIZE(otg_udc_pads));
}

#endif /*CONFIG_IMX_UDC*/
