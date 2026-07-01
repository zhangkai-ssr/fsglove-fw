#include <string.h>
#include "imu_node.h"
#include "fsglove_config.h"
#include "../Drivers/imu/lsm6dsox.h"
#include "../Drivers/imu/lis3mdl.h"

#define FSG_BOARD_IMU_NODE 0u

static I2C_HandleTypeDef *s_hi2c;
static uint8_t s_online[FSG_NODE_COUNT];
static uint8_t s_mag_online;

static lsm6dsox_t board_imu(void)
{
    lsm6dsox_t dev = {
        .hi2c = s_hi2c,
        .addr = LSM6DSOX_I2C_ADDR_LOW,
    };
    return dev;
}

static lis3mdl_t board_mag(void)
{
    lis3mdl_t dev = {
        .hi2c = s_hi2c,
        .addr = LIS3MDL_I2C_ADDR_LOW,
    };
    return dev;
}

void imu_node_layer_init(I2C_HandleTypeDef *hi2c)
{
    s_hi2c = hi2c;
    memset(s_online, 0, sizeof(s_online));
    s_mag_online = 0u;
}

int imu_node_probe(void)
{
    if (s_hi2c == 0) {
        return 0;
    }

    lsm6dsox_t dev = board_imu();
    if (lsm6dsox_whoami(&dev) == LSM6DSOX_WHO_AM_I_VAL &&
        lsm6dsox_init(&dev, FSG_ODR_HZ) == 0) {
        s_online[FSG_BOARD_IMU_NODE] = 1u;
        if (FSG_ENABLE_MAG && lsm6dsox_enable_pass_through(&dev, 1) == 0) {
            lis3mdl_t mag = board_mag();
            HAL_Delay(5);
            if (lis3mdl_whoami(&mag) == LIS3MDL_WHO_AM_I_VAL &&
                lis3mdl_init(&mag) == 0) {
                s_mag_online = 1u;
            }
        }
        return 1;
    }
    return 0;
}

int imu_node_read(uint8_t node_idx, fsg_node_data_t *out)
{
    if (node_idx >= FSG_NODE_COUNT || out == 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->node_id = node_idx;

    if (!s_online[node_idx]) {
        return 0;
    }

    out->status = FSG_ST_NODE_ONLINE;
    if (node_idx == FSG_BOARD_IMU_NODE) {
        lsm6dsox_t dev = board_imu();
        if (lsm6dsox_read_accel_gyro(&dev, out->accel, out->gyro) == 0) {
            out->status |= FSG_ST_ACCEL_OK | FSG_ST_GYRO_OK | FSG_ST_DATA_FRESH;
        }
        if (FSG_ENABLE_MAG && s_mag_online) {
            lis3mdl_t mag = board_mag();
            if (lis3mdl_read_mag(&mag, out->mag) == 0) {
                out->status |= FSG_ST_MAG_OK;
            }
        }
    }
    return 0;
}
