/*
 * LSM6DSOX 6-axis IMU driver for the current control-board netlist.
 *
 * Netlist_控制主板_2026-07-01.tel:
 *   U4.13 I2C_SCL_G -> U2.H3 -> PF1/I2C2_SCL
 *   U4.14 I2C_SDA_G -> U2.E2 -> PF0/I2C2_SDA
 *   U4.1  GND       -> SA0 low, 7-bit address 0x6A
 */
#ifndef LSM6DSOX_H
#define LSM6DSOX_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LSM6DSOX_I2C_ADDR_LOW   (0x6Au << 1)

#define LSM6DSOX_REG_WHO_AM_I   0x0Fu
#define LSM6DSOX_WHO_AM_I_VAL   0x6Cu

#define LSM6DSOX_REG_CTRL1_XL   0x10u
#define LSM6DSOX_REG_CTRL2_G    0x11u
#define LSM6DSOX_REG_CTRL3_C    0x12u
#define LSM6DSOX_REG_OUTX_L_G   0x22u

#define LSM6DSOX_ODR_104HZ      0x40u
#define LSM6DSOX_ODR_208HZ      0x50u
#define LSM6DSOX_XL_FS_4G       0x08u
#define LSM6DSOX_G_FS_2000DPS   0x0Cu
#define LSM6DSOX_CTRL3_BDU      0x40u
#define LSM6DSOX_CTRL3_IF_INC   0x04u
#define LSM6DSOX_CTRL3_SW_RESET 0x01u

#define LSM6DSOX_REG_FUNC_CFG_ACCESS 0x01u
#define LSM6DSOX_FUNC_CFG_SHUB       0x40u
#define LSM6DSOX_SHUB_MASTER_CONFIG  0x14u
#define LSM6DSOX_SHUB_PASS_THROUGH   0x10u

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t addr;
} lsm6dsox_t;

int lsm6dsox_whoami(const lsm6dsox_t *dev);
int lsm6dsox_init(const lsm6dsox_t *dev, uint16_t odr_hz);
int lsm6dsox_enable_pass_through(const lsm6dsox_t *dev, int enable);
int lsm6dsox_read_accel_gyro(const lsm6dsox_t *dev, int16_t accel[3], int16_t gyro[3]);

#ifdef __cplusplus
}
#endif
#endif /* LSM6DSOX_H */
