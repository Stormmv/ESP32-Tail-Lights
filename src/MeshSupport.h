#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <string.h>

inline String formatTransportAddress(const TransportAddress &address)
{
  if (!address.isValid())
  {
    return "---";
  }

  String result;
  for (uint8_t i = 0; i < address.length; i++)
  {
    if (i > 0)
    {
      result += ":";
    }

    if (address.bytes[i] < 16)
    {
      result += "0";
    }

    result += String(address.bytes[i], HEX);
  }

  result.toUpperCase();
  return result;
}

inline void copyTransportAddressToMac(const TransportAddress &address, uint8_t out[6])
{
  memset(out, 0, 6);
  if (!address.isValid())
  {
    return;
  }

  uint8_t copyLen = address.length < 6 ? address.length : 6;
  memcpy(out, address.data(), copyLen);
}

inline uint8_t toLegacySyncModeValue(SyncMode mode)
{
  switch (mode)
  {
  case SyncMode::SOLO:
    return 0;
  case SyncMode::JOIN:
    return 1;
  case SyncMode::HOST:
    return 2;
  case SyncMode::AUTO:
    return 1;
  default:
    return 0;
  }
}

inline SyncMode fromLegacySyncModeValue(uint8_t modeValue)
{
  switch (modeValue)
  {
  case 1:
    return SyncMode::JOIN;
  case 2:
    return SyncMode::HOST;
  default:
    return SyncMode::SOLO;
  }
}
