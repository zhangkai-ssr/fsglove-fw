/*
 * LIS3MDL magnetometer driver for the current control-board netlist.
 *
 * U3 is reached through the LSM6DSOX auxiliary I2C pass-through path:
 *   U3.1  3IMU_SCL1 -> U4.3
 *   U3.11 3IMU_SDA1 -> U4.2
 *   U3.9  GND       -> SA1 low, 7-bit address 0x1C
 */
#ifndef LIS3MDL_H
#define LIS3MDL_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIS3MDL_I2C_ADDR_LOW    (0x1Cu << 1)

#define LIS3MDL_REG_WHO_AM_I    0x0Fu
#define LIS3MDL_WHO_AM_I_VAL    0x3Du

#define LIS3MDL_REG_CTRL1       0x20u
#define LIS3MDL_REG_CTRL2       0x21u
#define LIS3MDL_REG_CTRL3       0x22u
#define LIS3MDL_REG_CTRL4       0x23u
#define LIS3MDL_REG_CTRL5       0x24u
#define LIS3MDL_REG_OUT_X_L     0x28u
#define LIS3MDL_I2C_AUTO_INC    0x80u

#define LIS3MDL_CTRL1_HP_80HZ   0x70u
#define LIS3MDL_CTRL2_FS_4GAUSS 0x00u
#define LIS3MDL_CTRL3_CONT      0x00u
#define LIS3MDL_CTRL4_Z_HP      0x0Cu
#define LIS3MDL_CTRL5_BDU       0x40u

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t addr;
} lis3mdl_t;

int lis3mdl_whoami(const lis3mdl_t *dev);
int lis3mdl_init(const lis3mdl_t *dev);
int lis3mdl_read_mag(const lis3mdl_t *dev, int16_t mag[3]);

#ifdef __cplusplus
}
#endif
#endif /* LIS3MDL_H */
