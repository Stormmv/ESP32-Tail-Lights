#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>
#include <stddef.h>

class Application;

constexpr uint8_t BLE_EFFECTS_PROTOCOL_VERSION = 2;
constexpr size_t BLE_EFFECTS_CHUNK_SIZE = 180;

enum class BLEEffectParamType : uint8_t
{
  BOOLEAN,
  INTEGER,
  FLOAT,
  ENUM,
  RGB
};

enum BLEEffectStripFlags : uint8_t
{
  BLE_STRIP_HEADLIGHT = 1 << 0,
  BLE_STRIP_TAILLIGHT = 1 << 1,
  BLE_STRIP_UNDERGLOW = 1 << 2,
  BLE_STRIP_INTERIOR = 1 << 3
};

struct BLEEffectEnumOption
{
  const char *id;
  const char *label;
  int value;
};

struct BLEEffectParamDescriptor
{
  const char *key;
  const char *label;
  BLEEffectParamType type;
  bool writable;
  bool hasRange;
  float minValue;
  float maxValue;
  float step;
  const BLEEffectEnumOption *options;
  size_t optionCount;
  void (*appendState)(Application &app, JsonObject params);
  bool (*applyValue)(Application &app, JsonVariantConst value, String &error);
};

struct BLEEffectDescriptor
{
  const char *id;
  const char *label;
  const char *category;
  uint8_t supportedStrips;
  const BLEEffectParamDescriptor *params;
  size_t paramCount;
};

class BLEEffectsCatalog
{
public:
  static const BLEEffectDescriptor *getEffects(size_t &count);
  static const BLEEffectDescriptor *findEffect(const char *effectId);

  static String buildCatalogJson();
  static String buildStateJson(Application &app);
  static bool applyCommand(Application &app, const String &commandJson, String &responseJson);

  static uint32_t getCatalogSchemaHash();
  static size_t getCatalogChunkCount();
  static size_t getChunkCountForPayload(const String &payload);
  static String getCatalogChunk(size_t chunkIndex);
  static String getPayloadChunk(const String &payload, size_t chunkIndex);
};
