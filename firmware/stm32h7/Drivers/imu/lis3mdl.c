#include "lis3mdl.h"

#define LIS3MDL_TIMEOUT_MS 10u

static int write_reg(const lis3mdl_t *dev, uint8_t reg, uint8_t val)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(dev->hi2c, dev->addr, reg,
                                             I2C_MEMADD_SIZE_8BIT, &val, 1,
                                             LIS3MDL_TIMEOUT_MS);
    return (st == HAL_OK) ? 0 : -1;
}

static int read_regs(const lis3mdl_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(dev->hi2c, dev->addr, reg,
                                            I2C_MEMADD_SIZE_8BIT, buf, len,
                                            LIS3MDL_TIMEOUT_MS);
    return (st == HAL_OK) ? 0 : -1;
}

int lis3mdl_whoami(const lis3mdl_t *dev)
{
    uint8_t v = 0;
    if (read_regs(dev, LIS3MDL_REG_WHO_AM_I, &v, 1) != 0) {
        return -1;
    }
    return (int)v;
}

int lis3mdl_init(const lis3mdl_t *dev)
{
    if (write_reg(dev, LIS3MDL_REG_CTRL1, LIS3MDL_CTRL1_HP_80HZ) != 0) {
        return -1;
    }
    if (write_reg(dev, LIS3MDL_REG_CTRL2, LIS3MDL_CTRL2_FS_4GAUSS) != 0) {
        return -1;
    }
    if (write_reg(dev, LIS3MDL_REG_CTRL4, LIS3MDL_CTRL4_Z_HP) != 0) {
        return -1;
    }
    if (write_reg(dev, LIS3MDL_REG_CTRL5, LIS3MDL_CTRL5_BDU) != 0) {
        return -1;
    }
    return write_reg(dev, LIS3MDL_REG_CTRL3, LIS3MDL_CTRL3_CONT);
}

int lis3mdl_read_mag(const lis3mdl_t *dev, int16_t mag[3])
{
    uint8_t raw[6];
    if (read_regs(dev, (uint8_t)(LIS3MDL_REG_OUT_X_L | LIS3MDL_I2C_AUTO_INC),
                  raw, sizeof(raw)) != 0) {
        return -1;
    }

    mag[0] = (int16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
    mag[1] = (int16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8));
    mag[2] = (int16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
    return 0;
}
