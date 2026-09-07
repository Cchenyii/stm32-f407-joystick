#include "sensor_protocol.h"

#include <string.h>

static void writeU16Be(uint8_t *out, uint16_t value)
{
  out[0] = (uint8_t)(value >> 8);
  out[1] = (uint8_t)value;
}

static void writeU32Be(uint8_t *out, uint32_t value)
{
  out[0] = (uint8_t)(value >> 24);
  out[1] = (uint8_t)(value >> 16);
  out[2] = (uint8_t)(value >> 8);
  out[3] = (uint8_t)value;
}

static uint16_t readU16Be(const uint8_t *data)
{
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t readU32Be(const uint8_t *data)
{
  return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
         ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

uint16_t Sp_Crc16Ccitt(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i)
  {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                            : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static size_t encodeFrame(uint8_t type, uint32_t sequence, const uint8_t *payload,
                          uint16_t payloadLength, uint8_t *output,
                          size_t outputCapacity)
{
  if (output == NULL || payloadLength > SP_MAX_PAYLOAD_SIZE ||
      (payloadLength > 0 && payload == NULL))
  {
    return 0;
  }

  const size_t frameLength = SP_FIXED_HEADER_SIZE + payloadLength + 2u;
  if (outputCapacity < frameLength)
  {
    return 0;
  }

  output[0] = SP_MAGIC_1;
  output[1] = SP_MAGIC_2;
  output[2] = SP_VERSION;
  output[3] = type;
  writeU32Be(output + 4, sequence);
  writeU16Be(output + 8, payloadLength);
  if (payloadLength > 0)
  {
    memcpy(output + SP_FIXED_HEADER_SIZE, payload, payloadLength);
  }

  const uint16_t crc =
      Sp_Crc16Ccitt(output + 2, SP_FIXED_HEADER_SIZE - 2u + payloadLength);
  writeU16Be(output + SP_FIXED_HEADER_SIZE + payloadLength, crc);
  return frameLength;
}

size_t Sp_EncodeJoystickFrame(uint32_t sequence, uint16_t x, uint16_t y,
                              uint8_t direction, uint8_t sw, uint8_t *output,
                              size_t outputCapacity)
{
  uint8_t payload[6];
  writeU16Be(payload, x);
  writeU16Be(payload + 2, y);
  payload[4] = direction;
  payload[5] = sw ? 1u : 0u;
  return encodeFrame(SP_TYPE_JOYSTICK_DATA, sequence, payload, sizeof(payload),
                     output, outputCapacity);
}

int Sp_IsAckOk(const uint8_t *frame, size_t length, uint32_t expectedSeq)
{
  if (frame == NULL || length < 13u)
  {
    return 0;
  }
  if (frame[0] != SP_MAGIC_1 || frame[1] != SP_MAGIC_2 ||
      frame[2] != SP_VERSION || frame[3] != SP_TYPE_ACK)
  {
    return 0;
  }
  if (readU32Be(frame + 4) != expectedSeq)
  {
    return 0;
  }
  if (readU16Be(frame + 8) != 1u)
  {
    return 0;
  }
  const uint16_t expectCrc = Sp_Crc16Ccitt(frame + 2, 9u);
  if (readU16Be(frame + 11) != expectCrc)
  {
    return 0;
  }
  return frame[10] == SP_ACK_OK;
}
