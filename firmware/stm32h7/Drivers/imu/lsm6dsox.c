#include "lsm6dsox.h"

#define LSM6DSOX_TIMEOUT_MS 10u

static int write_reg(const lsm6dsox_t *dev, uint8_t reg, uint8_t val)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(dev->hi2c, dev->addr, reg,
                                             I2C_MEMADD_SIZE_8BIT, &val, 1,
                                             LSM6DSOX_TIMEOUT_MS);
    return (st == HAL_OK) ? 0 : -1;
}

static int read_regs(const lsm6dsox_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(dev->hi2c, dev->addr, reg,
                                            I2C_MEMADD_SIZE_8BIT, buf, len,
                                            LSM6DSOX_TIMEOUT_MS);
    return (st == HAL_OK) ? 0 : -1;
}

int lsm6dsox_whoami(const lsm6dsox_t *dev)
{
    uint8_t v = 0;
    if (read_regs(dev, LSM6DSOX_REG_WHO_AM_I, &v, 1) != 0) {
        return -1;
    }
    return (int)v;
}

int lsm6dsox_init(const lsm6dsox_t *dev, uint16_t odr_hz)
{
    if (write_reg(dev, LSM6DSOX_REG_CTRL3_C, LSM6DSOX_CTRL3_SW_RESET) != 0) {
        return -1;
    }
    HAL_Delay(10);

    if (write_reg(dev, LSM6DSOX_REG_CTRL3_C,
                  LSM6DSOX_CTRL3_BDU | LSM6DSOX_CTRL3_IF_INC) != 0) {
        return -1;
    }

    uint8_t odr = (odr_hz >= 200u) ? LSM6DSOX_ODR_208HZ : LSM6DSOX_ODR_104HZ;
    if (write_reg(dev, LSM6DSOX_REG_CTRL1_XL, (uint8_t)(odr | LSM6DSOX_XL_FS_4G)) != 0) {
        return -1;
    }
    return write_reg(dev, LSM6DSOX_REG_CTRL2_G, (uint8_t)(odr | LSM6DSOX_G_FS_2000DPS));
}

int lsm6dsox_enable_pass_through(const lsm6dsox_t *dev, int enable)
{
    int rc;

    if (write_reg(dev, LSM6DSOX_REG_FUNC_CFG_ACCESS, LSM6DSOX_FUNC_CFG_SHUB) != 0) {
        return -1;
    }
    rc = write_reg(dev, LSM6DSOX_SHUB_MASTER_CONFIG,
                   enable ? LSM6DSOX_SHUB_PASS_THROUGH : 0u);
    if (write_reg(dev, LSM6DSOX_REG_FUNC_CFG_ACCESS, 0u) != 0) {
        return -1;
    }
    return rc;
}

int lsm6dsox_read_accel_gyro(const lsm6dsox_t *dev, int16_t accel[3], int16_t gyro[3])
{
    uint8_t raw[12];
    if (read_regs(dev, LSM6DSOX_REG_OUTX_L_G, raw, sizeof(raw)) != 0) {
        return -1;
    }

    gyro[0]  = (int16_t)((uint16_t)raw[0]  | ((uint16_t)raw[1]  << 8));
    gyro[1]  = (int16_t)((uint16_t)raw[2]  | ((uint16_t)raw[3]  << 8));
    gyro[2]  = (int16_t)((uint16_t)raw[4]  | ((uint16_t)raw[5]  << 8));
    accel[0] = (int16_t)((uint16_t)raw[6]  | ((uint16_t)raw[7]  << 8));
    accel[1] = (int16_t)((uint16_t)raw[8]  | ((uint16_t)raw[9]  << 8));
    accel[2] = (int16_t)((uint16_t)raw[10] | ((uint16_t)raw[11] << 8));
    return 0;
}
