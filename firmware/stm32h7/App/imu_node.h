#ifndef IMU_NODE_H
#define IMU_NODE_H

#include <stdint.h>
#include "stm32h7xx_hal.h"
#include "../../common/fsglove_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

void imu_node_layer_init(I2C_HandleTypeDef *hi2c);
int imu_node_probe(void);
int imu_node_read(uint8_t node_idx, fsg_node_data_t *out);

#ifdef __cplusplus
}
#endif
#endif /* IMU_NODE_H */
