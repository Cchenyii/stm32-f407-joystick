#ifndef SENSOR_PROTOCOL_H
#define SENSOR_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SP_MAGIC_1 0xAAu
#define SP_MAGIC_2 0x55u
#define SP_VERSION 0x01u
#define SP_FIXED_HEADER_SIZE 10u
#define SP_MAX_PAYLOAD_SIZE 64u
#define SP_MAX_FRAME_SIZE (SP_FIXED_HEADER_SIZE + SP_MAX_PAYLOAD_SIZE + 2u)

#define SP_TYPE_SENSOR_DATA 0x01u
#define SP_TYPE_ACK 0x02u
#define SP_TYPE_HEARTBEAT 0x03u
#define SP_TYPE_JOYSTICK_DATA 0x04u

#define SP_JOY_CENTER 0u
#define SP_JOY_LEFT 1u
#define SP_JOY_RIGHT 2u
#define SP_JOY_UP 3u
#define SP_JOY_DOWN 4u

#define SP_ACK_OK 0x00u

uint16_t Sp_Crc16Ccitt(const uint8_t *data, size_t length);

size_t Sp_EncodeJoystickFrame(uint32_t sequence, uint16_t x, uint16_t y,
                              uint8_t direction, uint8_t sw, uint8_t *output,
                              size_t outputCapacity);

/* Returns 1 if frame is ACK OK for expectedSeq */
int Sp_IsAckOk(const uint8_t *frame, size_t length, uint32_t expectedSeq);

#ifdef __cplusplus
}
#endif

#endif
