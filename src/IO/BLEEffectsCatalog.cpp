#include "BLEEffectsCatalog.h"

#include "../Application.h"
#include "LED/LEDStrip.h"

namespace
{
template <typename T, size_t N>
constexpr size_t countOf(const T (&)[N])
{
  return N;
}

const char *stripFlagToName(uint8_t flag)
{
  switch (flag)
  {
  case BLE_STRIP_HEADLIGHT:
    return "headlight";
  case BLE_STRIP_TAILLIGHT:
    return "taillight";
  case BLE_STRIP_UNDERGLOW:
    return "underglow";
  case BLE_STRIP_INTERIOR:
    return "interior";
  default:
    return "unknown";
  }
}

void appendSupportedStrips(JsonArray strips, uint8_t supportedStrips)
{
  const uint8_t allFlags[] = {
      BLE_STRIP_HEADLIGHT,
      BLE_STRIP_TAILLIGHT,
      BLE_STRIP_UNDERGLOW,
      BLE_STRIP_INTERIOR};

  for (uint8_t flag : allFlags)
  {
    if ((supportedStrips & flag) != 0)
    {
      strips.add(stripFlagToName(flag));
    }
  }
}

bool parseBool(JsonVariantConst value, bool &out, String &error)
{
  if (!value.is<bool>())
  {
    error = "expected boolean";
    return false;
  }

  out = value.as<bool>();
  return true;
}

bool parseInt(JsonVariantConst value, int &out, String &error)
{
  if (!value.is<int>() && !value.is<long>() && !value.is<float>())
  {
    error = "expected integer";
    return false;
  }

  out = value.as<int>();
  return true;
}

bool parseUint16(JsonVariantConst value, uint16_t &out, String &error)
{
  int parsed = 0;
  if (!parseInt(value, parsed, error))
  {
    return false;
  }

  if (parsed < 0 || parsed > 65535)
  {
    error = "integer out of range";
    return false;
  }

  out = static_cast<uint16_t>(parsed);
  return true;
}

bool parseUint32(JsonVariantConst value, uint32_t &out, String &error)
{
  if (!value.is<unsigned long>() && !value.is<unsigned int>() && !value.is<long>() && !value.is<int>() && !value.is<float>())
  {
    error = "expected integer";
    return false;
  }

  long parsed = value.as<long>();
  if (parsed < 0)
  {
    error = "integer out of range";
    return false;
  }

  out = static_cast<uint32_t>(parsed);
  return true;
}

bool parseFloat(JsonVariantConst value, float &out, String &error)
{
  if (!value.is<float>() && !value.is<double>() && !value.is<int>() && !value.is<long>())
  {
    error = "expected number";
    return false;
  }

  out = value.as<float>();
  return true;
}

bool parseRgb(JsonVariantConst value, Color &out, String &error)
{
  if (!value.is<JsonObjectConst>())
  {
    error = "expected rgb object";
    return false;
  }

  JsonObjectConst color = value.as<JsonObjectConst>();
  if (!color["r"].is<int>() || !color["g"].is<int>() || !color["b"].is<int>())
  {
    error = "rgb object must contain integer r, g, b";
    return false;
  }

  int r = color["r"].as<int>();
  int g = color["g"].as<int>();
  int b = color["b"].as<int>();
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
  {
    error = "rgb values must be between 0 and 255";
    return false;
  }

  out = Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
  return true;
}

const BLEEffectEnumOption *findEnumOption(const BLEEffectEnumOption *options, size_t optionCount, const char *id)
{
  for (size_t i = 0; i < optionCount; ++i)
  {
    if (strcmp(options[i].id, id) == 0)
    {
      return &options[i];
    }
  }

  return nullptr;
}

const char *getEnumIdByValue(const BLEEffectEnumOption *options, size_t optionCount, int value)
{
  for (size_t i = 0; i < optionCount; ++i)
  {
    if (options[i].value == value)
    {
      return options[i].id;
    }
  }

  return optionCount > 0 ? options[0].id : "";
}

bool parseEnumId(JsonVariantConst value, const BLEEffectEnumOption *options, size_t optionCount, int &out, String &error)
{
  if (!value.is<const char *>())
  {
    error = "expected enum id";
    return false;
  }

  const char *id = value.as<const char *>();
  const BLEEffectEnumOption *option = findEnumOption(options, optionCount, id);
  if (option == nullptr)
  {
    error = String("unknown enum option: ") + id;
    return false;
  }

  out = option->value;
  return true;
}

void appendRgb(JsonObject params, const char *key, const Color &color)
{
  JsonObject rgb = params.createNestedObject(key);
  rgb["r"] = color.r;
  rgb["g"] = color.g;
  rgb["b"] = color.b;
}

void appendBoolColor(JsonObject params, const char *key, bool r, bool g, bool b)
{
  appendRgb(params, key, Color(r ? 255 : 0, g ? 255 : 0, b ? 255 : 0));
}

bool applyBoolColor(JsonVariantConst value, bool &r, bool &g, bool &b, String &error)
{
  Color color;
  if (!parseRgb(value, color, error))
  {
    return false;
  }

  r = color.r > 0;
  g = color.g > 0;
  b = color.b > 0;
  return true;
}

const BLEEffectEnumOption headlightModeOptions[] = {
    {"off", "Off", static_cast<int>(HeadlightEffectMode::Off)},
    {"startup", "Startup", static_cast<int>(HeadlightEffectMode::Startup)},
    {"car_on", "Car On", static_cast<int>(HeadlightEffectMode::CarOn)},
};

const BLEEffectEnumOption taillightModeOptions[] = {
    {"off", "Off", static_cast<int>(TaillightEffectMode::Off)},
    {"startup", "Startup", static_cast<int>(TaillightEffectMode::Startup)},
    {"car_on", "Car On", static_cast<int>(TaillightEffectMode::CarOn)},
    {"dim", "Dim", static_cast<int>(TaillightEffectMode::Dim)},
};

const BLEEffectEnumOption policeModeOptions[] = {
    {"slow", "Slow", static_cast<int>(PoliceMode::SLOW)},
    {"fast", "Fast", static_cast<int>(PoliceMode::FAST)},
};

const BLEEffectEnumOption serviceLightsModeOptions[] = {
    {"slow", "Slow", static_cast<int>(ServiceLightsMode::SLOW)},
    {"fast", "Fast", static_cast<int>(ServiceLightsMode::FAST)},
    {"alternate", "Alternate", static_cast<int>(ServiceLightsMode::ALTERNATE)},
    {"strobe", "Strobe", static_cast<int>(ServiceLightsMode::STROBE)},
    {"scroll", "Scroll", static_cast<int>(ServiceLightsMode::SCROLL)},
};

const BLEEffectEnumOption solidColorPresetOptions[] = {
    {"off", "Off", static_cast<int>(SolidColorPreset::OFF)},
    {"red", "Red", static_cast<int>(SolidColorPreset::RED)},
    {"green", "Green", static_cast<int>(SolidColorPreset::GREEN)},
    {"blue", "Blue", static_cast<int>(SolidColorPreset::BLUE)},
    {"white", "White", static_cast<int>(SolidColorPreset::WHITE)},
    {"yellow", "Yellow", static_cast<int>(SolidColorPreset::YELLOW)},
    {"cyan", "Cyan", static_cast<int>(SolidColorPreset::CYAN)},
    {"magenta", "Magenta", static_cast<int>(SolidColorPreset::MAGENTA)},
    {"orange", "Orange", static_cast<int>(SolidColorPreset::ORANGE)},
    {"purple", "Purple", static_cast<int>(SolidColorPreset::PURPLE)},
    {"lime", "Lime", static_cast<int>(SolidColorPreset::LIME)},
    {"pink", "Pink", static_cast<int>(SolidColorPreset::PINK)},
    {"teal", "Teal", static_cast<int>(SolidColorPreset::TEAL)},
    {"indigo", "Indigo", static_cast<int>(SolidColorPreset::INDIGO)},
    {"gold", "Gold", static_cast<int>(SolidColorPreset::GOLD)},
    {"silver", "Silver", static_cast<int>(SolidColorPreset::SILVER)},
    {"custom", "Custom", static_cast<int>(SolidColorPreset::CUSTOM)},
};
void writeLeftIndicatorActive(Application &app, JsonObject params)
{
  params["active"] = app.leftIndicatorEffect ? app.leftIndicatorEffect->isActive() : false;
}

bool applyLeftIndicatorActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.leftIndicatorEffect == nullptr)
  {
    error = "left_indicator unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.leftIndicatorEffect->setActive(active);
  return true;
}

void writeRightIndicatorActive(Application &app, JsonObject params)
{
  params["active"] = app.rightIndicatorEffect ? app.rightIndicatorEffect->isActive() : false;
}

bool applyRightIndicatorActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.rightIndicatorEffect == nullptr)
  {
    error = "right_indicator unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.rightIndicatorEffect->setActive(active);
  return true;
}

void writeHeadlightMode(Application &app, JsonObject params)
{
  int mode = app.headlightEffect ? static_cast<int>(app.headlightEffect->getMode()) : static_cast<int>(HeadlightEffectMode::Off);
  params["mode"] = getEnumIdByValue(headlightModeOptions, countOf(headlightModeOptions), mode);
}

bool applyHeadlightMode(Application &app, JsonVariantConst value, String &error)
{
  if (app.headlightEffect == nullptr)
  {
    error = "headlight unavailable";
    return false;
  }

  int mode = 0;
  if (!parseEnumId(value, headlightModeOptions, countOf(headlightModeOptions), mode, error))
  {
    return false;
  }

  app.headlightEffect->setMode(mode);
  return true;
}

void writeHeadlightSplit(Application &app, JsonObject params)
{
  params["split"] = app.headlightEffect ? app.headlightEffect->getSplit() : false;
}

bool applyHeadlightSplit(Application &app, JsonVariantConst value, String &error)
{
  if (app.headlightEffect == nullptr)
  {
    error = "headlight unavailable";
    return false;
  }

  bool split = false;
  if (!parseBool(value, split, error))
  {
    return false;
  }

  app.headlightEffect->setSplit(split);
  return true;
}

void writeHeadlightColor(Application &app, JsonObject params)
{
  bool r = false;
  bool g = false;
  bool b = false;
  if (app.headlightEffect != nullptr)
  {
    app.headlightEffect->getColor(r, g, b);
  }
  appendBoolColor(params, "color", r, g, b);
}

bool applyHeadlightColor(Application &app, JsonVariantConst value, String &error)
{
  if (app.headlightEffect == nullptr)
  {
    error = "headlight unavailable";
    return false;
  }

  bool r = false;
  bool g = false;
  bool b = false;
  if (!applyBoolColor(value, r, g, b, error))
  {
    return false;
  }

  app.headlightEffect->setColor(r, g, b);
  return true;
}

void writeTaillightMode(Application &app, JsonObject params)
{
  int mode = app.taillightEffect ? static_cast<int>(app.taillightEffect->getMode()) : static_cast<int>(TaillightEffectMode::Off);
  params["mode"] = getEnumIdByValue(taillightModeOptions, countOf(taillightModeOptions), mode);
}

bool applyTaillightMode(Application &app, JsonVariantConst value, String &error)
{
  if (app.taillightEffect == nullptr)
  {
    error = "taillight unavailable";
    return false;
  }

  int mode = 0;
  if (!parseEnumId(value, taillightModeOptions, countOf(taillightModeOptions), mode, error))
  {
    return false;
  }

  app.taillightEffect->setMode(mode);
  return true;
}

void writeTaillightSplit(Application &app, JsonObject params)
{
  params["split"] = app.taillightEffect ? app.taillightEffect->getSplit() : false;
}

bool applyTaillightSplit(Application &app, JsonVariantConst value, String &error)
{
  if (app.taillightEffect == nullptr)
  {
    error = "taillight unavailable";
    return false;
  }

  bool split = false;
  if (!parseBool(value, split, error))
  {
    return false;
  }

  app.taillightEffect->setSplit(split);
  return true;
}

void writeRgbActive(Application &app, JsonObject params)
{
  params["active"] = app.rgbEffect ? app.rgbEffect->isActive() : false;
}

bool applyRgbActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.rgbEffect == nullptr)
  {
    error = "rgb unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.rgbEffect->setActive(active);
  return true;
}

void writeRgbBaseHueCenter(Application &app, JsonObject params)
{
  params["baseHueCenter"] = app.rgbEffect ? app.rgbEffect->baseHueCenter : 0.0f;
}

bool applyRgbBaseHueCenter(Application &app, JsonVariantConst value, String &error)
{
  if (app.rgbEffect == nullptr)
  {
    error = "rgb unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.rgbEffect->baseHueCenter = parsed;
  return true;
}

void writeRgbBaseHueEdge(Application &app, JsonObject params)
{
  params["baseHueEdge"] = app.rgbEffect ? app.rgbEffect->baseHueEdge : 0.0f;
}

bool applyRgbBaseHueEdge(Application &app, JsonVariantConst value, String &error)
{
  if (app.rgbEffect == nullptr)
  {
    error = "rgb unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.rgbEffect->baseHueEdge = parsed;
  return true;
}

void writeRgbSpeed(Application &app, JsonObject params)
{
  params["speed"] = app.rgbEffect ? app.rgbEffect->speed : 0.0f;
}

bool applyRgbSpeed(Application &app, JsonVariantConst value, String &error)
{
  if (app.rgbEffect == nullptr)
  {
    error = "rgb unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.rgbEffect->speed = parsed;
  return true;
}

void writeNightRiderActive(Application &app, JsonObject params)
{
  params["active"] = app.nightriderEffect ? app.nightriderEffect->isActive() : false;
}

bool applyNightRiderActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.nightriderEffect == nullptr)
  {
    error = "nightrider unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.nightriderEffect->setActive(active);
  return true;
}

void writeNightRiderCycleTime(Application &app, JsonObject params)
{
  params["cycleTime"] = app.nightriderEffect ? app.nightriderEffect->cycleTime : 0.0f;
}

bool applyNightRiderCycleTime(Application &app, JsonVariantConst value, String &error)
{
  if (app.nightriderEffect == nullptr)
  {
    error = "nightrider unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.nightriderEffect->cycleTime = parsed;
  return true;
}

void writeNightRiderTailLength(Application &app, JsonObject params)
{
  params["tailLength"] = app.nightriderEffect ? app.nightriderEffect->tailLength : 0.0f;
}

bool applyNightRiderTailLength(Application &app, JsonVariantConst value, String &error)
{
  if (app.nightriderEffect == nullptr)
  {
    error = "nightrider unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.nightriderEffect->tailLength = parsed;
  return true;
}

void writePoliceActive(Application &app, JsonObject params)
{
  params["active"] = app.policeEffect ? app.policeEffect->isActive() : false;
}

bool applyPoliceActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.policeEffect == nullptr)
  {
    error = "police unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.policeEffect->setActive(active);
  return true;
}

void writePoliceMode(Application &app, JsonObject params)
{
  int mode = app.policeEffect ? static_cast<int>(app.policeEffect->getMode()) : static_cast<int>(PoliceMode::FAST);
  params["mode"] = getEnumIdByValue(policeModeOptions, countOf(policeModeOptions), mode);
}

bool applyPoliceMode(Application &app, JsonVariantConst value, String &error)
{
  if (app.policeEffect == nullptr)
  {
    error = "police unavailable";
    return false;
  }

  int mode = 0;
  if (!parseEnumId(value, policeModeOptions, countOf(policeModeOptions), mode, error))
  {
    return false;
  }

  app.policeEffect->setMode(static_cast<PoliceMode>(mode));
  return true;
}

void writePulseWaveActive(Application &app, JsonObject params)
{
  params["active"] = app.pulseWaveEffect ? app.pulseWaveEffect->isActive() : false;
}

bool applyPulseWaveActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.pulseWaveEffect == nullptr)
  {
    error = "pulse_wave unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.pulseWaveEffect->setActive(active);
  return true;
}

void writePulseWaveSpeed(Application &app, JsonObject params)
{
  params["waveSpeed"] = app.pulseWaveEffect ? app.pulseWaveEffect->waveSpeed : 0.0f;
}

bool applyPulseWaveSpeed(Application &app, JsonVariantConst value, String &error)
{
  if (app.pulseWaveEffect == nullptr)
  {
    error = "pulse_wave unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.pulseWaveEffect->waveSpeed = parsed;
  return true;
}

void writePulseWaveFrequency(Application &app, JsonObject params)
{
  params["pulseFrequency"] = app.pulseWaveEffect ? app.pulseWaveEffect->pulseFrequency : 0.0f;
}

bool applyPulseWaveFrequency(Application &app, JsonVariantConst value, String &error)
{
  if (app.pulseWaveEffect == nullptr)
  {
    error = "pulse_wave unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.pulseWaveEffect->pulseFrequency = parsed;
  return true;
}

void writeAuroraActive(Application &app, JsonObject params)
{
  params["active"] = app.auroraEffect ? app.auroraEffect->isActive() : false;
}

bool applyAuroraActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.auroraEffect == nullptr)
  {
    error = "aurora unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.auroraEffect->setActive(active);
  return true;
}

void writeAuroraMovementSpeed(Application &app, JsonObject params)
{
  params["movementSpeed"] = app.auroraEffect ? app.auroraEffect->movementSpeed : 0.0f;
}

bool applyAuroraMovementSpeed(Application &app, JsonVariantConst value, String &error)
{
  if (app.auroraEffect == nullptr)
  {
    error = "aurora unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.auroraEffect->movementSpeed = parsed;
  return true;
}

void writeAuroraWaveIntensity(Application &app, JsonObject params)
{
  params["waveIntensity"] = app.auroraEffect ? app.auroraEffect->waveIntensity : 0.0f;
}

bool applyAuroraWaveIntensity(Application &app, JsonVariantConst value, String &error)
{
  if (app.auroraEffect == nullptr)
  {
    error = "aurora unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.auroraEffect->waveIntensity = parsed;
  return true;
}

void writeSolidColorActive(Application &app, JsonObject params)
{
  params["active"] = app.solidColorEffect ? app.solidColorEffect->isActive() : false;
}

bool applySolidColorActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.solidColorEffect == nullptr)
  {
    error = "solid_color unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.solidColorEffect->setActive(active);
  return true;
}

void writeSolidColorPreset(Application &app, JsonObject params)
{
  int preset = app.solidColorEffect ? static_cast<int>(app.solidColorEffect->getColorPreset()) : static_cast<int>(SolidColorPreset::OFF);
  params["preset"] = getEnumIdByValue(solidColorPresetOptions, countOf(solidColorPresetOptions), preset);
}

bool applySolidColorPreset(Application &app, JsonVariantConst value, String &error)
{
  if (app.solidColorEffect == nullptr)
  {
    error = "solid_color unavailable";
    return false;
  }

  int preset = 0;
  if (!parseEnumId(value, solidColorPresetOptions, countOf(solidColorPresetOptions), preset, error))
  {
    return false;
  }

  app.solidColorEffect->setColorPreset(static_cast<SolidColorPreset>(preset));
  return true;
}

void writeSolidColorCustomColor(Application &app, JsonObject params)
{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  if (app.solidColorEffect != nullptr)
  {
    app.solidColorEffect->getCustomColor(r, g, b);
  }
  appendRgb(params, "customColor", Color(r, g, b));
}

bool applySolidColorCustomColor(Application &app, JsonVariantConst value, String &error)
{
  if (app.solidColorEffect == nullptr)
  {
    error = "solid_color unavailable";
    return false;
  }

  Color color;
  if (!parseRgb(value, color, error))
  {
    return false;
  }

  app.solidColorEffect->setCustomColor(color.r, color.g, color.b);
  return true;
}

void writeColorFadeActive(Application &app, JsonObject params)
{
  params["active"] = app.colorFadeEffect ? app.colorFadeEffect->isActive() : false;
}

bool applyColorFadeActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.colorFadeEffect == nullptr)
  {
    error = "color_fade unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.colorFadeEffect->setActive(active);
  return true;
}

void writeColorFadeHoldTime(Application &app, JsonObject params)
{
  params["holdTime"] = app.colorFadeEffect ? app.colorFadeEffect->holdTime : 0.0f;
}

bool applyColorFadeHoldTime(Application &app, JsonVariantConst value, String &error)
{
  if (app.colorFadeEffect == nullptr)
  {
    error = "color_fade unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.colorFadeEffect->holdTime = parsed;
  return true;
}

void writeColorFadeFadeTime(Application &app, JsonObject params)
{
  params["fadeTime"] = app.colorFadeEffect ? app.colorFadeEffect->fadeTime : 0.0f;
}

bool applyColorFadeFadeTime(Application &app, JsonVariantConst value, String &error)
{
  if (app.colorFadeEffect == nullptr)
  {
    error = "color_fade unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.colorFadeEffect->fadeTime = parsed;
  return true;
}

void writeCommitActive(Application &app, JsonObject params)
{
  params["active"] = app.commitEffect ? app.commitEffect->isActive() : false;
}

bool applyCommitActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.commitEffect == nullptr)
  {
    error = "commit unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.commitEffect->setActive(active);
  return true;
}

void writeCommitSpeed(Application &app, JsonObject params)
{
  params["commitSpeed"] = app.commitEffect ? app.commitEffect->commitSpeed : 0;
}

bool applyCommitSpeed(Application &app, JsonVariantConst value, String &error)
{
  if (app.commitEffect == nullptr)
  {
    error = "commit unavailable";
    return false;
  }

  uint32_t parsed = 0;
  if (!parseUint32(value, parsed, error))
  {
    return false;
  }

  app.commitEffect->commitSpeed = parsed;
  return true;
}

void writeCommitTrailLength(Application &app, JsonObject params)
{
  params["trailLength"] = app.commitEffect ? app.commitEffect->trailLength : 0;
}

bool applyCommitTrailLength(Application &app, JsonVariantConst value, String &error)
{
  if (app.commitEffect == nullptr)
  {
    error = "commit unavailable";
    return false;
  }

  uint16_t parsed = 0;
  if (!parseUint16(value, parsed, error))
  {
    return false;
  }

  app.commitEffect->trailLength = parsed;
  return true;
}

void writeCommitInterval(Application &app, JsonObject params)
{
  params["commitInterval"] = app.commitEffect ? app.commitEffect->commitInterval : 0;
}

bool applyCommitInterval(Application &app, JsonVariantConst value, String &error)
{
  if (app.commitEffect == nullptr)
  {
    error = "commit unavailable";
    return false;
  }

  uint32_t parsed = 0;
  if (!parseUint32(value, parsed, error))
  {
    return false;
  }

  app.commitEffect->commitInterval = parsed;
  return true;
}

void writeCommitHeadColor(Application &app, JsonObject params)
{
  Color color;
  if (app.commitEffect != nullptr)
  {
    color = Color(app.commitEffect->headR, app.commitEffect->headG, app.commitEffect->headB);
  }
  appendRgb(params, "headColor", color);
}

bool applyCommitHeadColor(Application &app, JsonVariantConst value, String &error)
{
  if (app.commitEffect == nullptr)
  {
    error = "commit unavailable";
    return false;
  }

  Color color;
  if (!parseRgb(value, color, error))
  {
    return false;
  }

  app.commitEffect->headR = color.r;
  app.commitEffect->headG = color.g;
  app.commitEffect->headB = color.b;
  return true;
}

void writeServiceLightsActive(Application &app, JsonObject params)
{
  params["active"] = app.serviceLightsEffect ? app.serviceLightsEffect->isActive() : false;
}

bool applyServiceLightsActive(Application &app, JsonVariantConst value, String &error)
{
  if (app.serviceLightsEffect == nullptr)
  {
    error = "service_lights unavailable";
    return false;
  }

  bool active = false;
  if (!parseBool(value, active, error))
  {
    return false;
  }

  app.serviceLightsEffect->setActive(active);
  return true;
}

void writeServiceLightsMode(Application &app, JsonObject params)
{
  int mode = app.serviceLightsEffect ? static_cast<int>(app.serviceLightsEffect->getMode()) : static_cast<int>(ServiceLightsMode::FAST);
  params["mode"] = getEnumIdByValue(serviceLightsModeOptions, countOf(serviceLightsModeOptions), mode);
}

bool applyServiceLightsMode(Application &app, JsonVariantConst value, String &error)
{
  if (app.serviceLightsEffect == nullptr)
  {
    error = "service_lights unavailable";
    return false;
  }

  int mode = 0;
  if (!parseEnumId(value, serviceLightsModeOptions, countOf(serviceLightsModeOptions), mode, error))
  {
    return false;
  }

  app.serviceLightsEffect->setMode(static_cast<ServiceLightsMode>(mode));
  return true;
}

void writeServiceLightsColor(Application &app, JsonObject params)
{
  appendRgb(params, "color", app.serviceLightsEffect ? app.serviceLightsEffect->getColor() : Color());
}

bool applyServiceLightsColor(Application &app, JsonVariantConst value, String &error)
{
  if (app.serviceLightsEffect == nullptr)
  {
    error = "service_lights unavailable";
    return false;
  }

  Color color;
  if (!parseRgb(value, color, error))
  {
    return false;
  }

  app.serviceLightsEffect->setColor(color);
  return true;
}

void writeServiceLightsFastSpeed(Application &app, JsonObject params)
{
  params["fastSpeed"] = app.serviceLightsEffect ? app.serviceLightsEffect->getFastSpeed() : 0.0f;
}

bool applyServiceLightsFastSpeed(Application &app, JsonVariantConst value, String &error)
{
  if (app.serviceLightsEffect == nullptr)
  {
    error = "service_lights unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.serviceLightsEffect->setFastSpeed(parsed);
  return true;
}

void writeServiceLightsSlowSpeed(Application &app, JsonObject params)
{
  params["slowSpeed"] = app.serviceLightsEffect ? app.serviceLightsEffect->getSlowSpeed() : 0.0f;
}

bool applyServiceLightsSlowSpeed(Application &app, JsonVariantConst value, String &error)
{
  if (app.serviceLightsEffect == nullptr)
  {
    error = "service_lights unavailable";
    return false;
  }

  float parsed = 0.0f;
  if (!parseFloat(value, parsed, error))
  {
    return false;
  }

  app.serviceLightsEffect->setSlowSpeed(parsed);
  return true;
}

void writeServiceLightsFlashes(Application &app, JsonObject params)
{
  params["flashesPerCycle"] = app.serviceLightsEffect ? app.serviceLightsEffect->getFastModeFlashesPerCycle() : 0;
}

bool applyServiceLightsFlashes(Application &app, JsonVariantConst value, String &error)
{
  if (app.serviceLightsEffect == nullptr)
  {
    error = "service_lights unavailable";
    return false;
  }

  uint16_t parsed = 0;
  if (!parseUint16(value, parsed, error))
  {
    return false;
  }

  app.serviceLightsEffect->setFastModeFlashesPerCycle(parsed);
  return true;
}

const BLEEffectParamDescriptor leftIndicatorParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeLeftIndicatorActive, applyLeftIndicatorActive},
};

const BLEEffectParamDescriptor rightIndicatorParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeRightIndicatorActive, applyRightIndicatorActive},
};

const BLEEffectParamDescriptor headlightParams[] = {
    {"mode", "Mode", BLEEffectParamType::ENUM, true, false, 0, 0, 0, headlightModeOptions, countOf(headlightModeOptions), writeHeadlightMode, applyHeadlightMode},
    {"split", "Split", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeHeadlightSplit, applyHeadlightSplit},
    {"color", "Color", BLEEffectParamType::RGB, true, false, 0, 0, 0, nullptr, 0, writeHeadlightColor, applyHeadlightColor},
};

const BLEEffectParamDescriptor taillightParams[] = {
    {"mode", "Mode", BLEEffectParamType::ENUM, true, false, 0, 0, 0, taillightModeOptions, countOf(taillightModeOptions), writeTaillightMode, applyTaillightMode},
    {"split", "Split", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeTaillightSplit, applyTaillightSplit},
};

const BLEEffectParamDescriptor rgbParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeRgbActive, applyRgbActive},
    {"baseHueCenter", "Base Hue Center", BLEEffectParamType::FLOAT, true, true, 0, 360, 1, nullptr, 0, writeRgbBaseHueCenter, applyRgbBaseHueCenter},
    {"baseHueEdge", "Base Hue Edge", BLEEffectParamType::FLOAT, true, true, 0, 360, 1, nullptr, 0, writeRgbBaseHueEdge, applyRgbBaseHueEdge},
    {"speed", "Speed", BLEEffectParamType::FLOAT, true, true, 0, 720, 1, nullptr, 0, writeRgbSpeed, applyRgbSpeed},
};

const BLEEffectParamDescriptor nightRiderParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeNightRiderActive, applyNightRiderActive},
    {"cycleTime", "Cycle Time", BLEEffectParamType::FLOAT, true, true, 0.1f, 20.0f, 0.1f, nullptr, 0, writeNightRiderCycleTime, applyNightRiderCycleTime},
    {"tailLength", "Tail Length", BLEEffectParamType::FLOAT, true, true, 0.1f, 100.0f, 0.1f, nullptr, 0, writeNightRiderTailLength, applyNightRiderTailLength},
};

const BLEEffectParamDescriptor policeParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writePoliceActive, applyPoliceActive},
    {"mode", "Mode", BLEEffectParamType::ENUM, true, false, 0, 0, 0, policeModeOptions, countOf(policeModeOptions), writePoliceMode, applyPoliceMode},
};

const BLEEffectParamDescriptor pulseWaveParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writePulseWaveActive, applyPulseWaveActive},
    {"waveSpeed", "Wave Speed", BLEEffectParamType::FLOAT, true, true, 0.0f, 20.0f, 0.1f, nullptr, 0, writePulseWaveSpeed, applyPulseWaveSpeed},
    {"pulseFrequency", "Pulse Frequency", BLEEffectParamType::FLOAT, true, true, 0.0f, 20.0f, 0.1f, nullptr, 0, writePulseWaveFrequency, applyPulseWaveFrequency},
};

const BLEEffectParamDescriptor auroraParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeAuroraActive, applyAuroraActive},
    {"movementSpeed", "Movement Speed", BLEEffectParamType::FLOAT, true, true, 0.0f, 20.0f, 0.1f, nullptr, 0, writeAuroraMovementSpeed, applyAuroraMovementSpeed},
    {"waveIntensity", "Wave Intensity", BLEEffectParamType::FLOAT, true, true, 0.0f, 10.0f, 0.1f, nullptr, 0, writeAuroraWaveIntensity, applyAuroraWaveIntensity},
};

const BLEEffectParamDescriptor solidColorParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeSolidColorActive, applySolidColorActive},
    {"preset", "Preset", BLEEffectParamType::ENUM, true, false, 0, 0, 0, solidColorPresetOptions, countOf(solidColorPresetOptions), writeSolidColorPreset, applySolidColorPreset},
    {"customColor", "Custom Color", BLEEffectParamType::RGB, true, false, 0, 0, 0, nullptr, 0, writeSolidColorCustomColor, applySolidColorCustomColor},
};

const BLEEffectParamDescriptor colorFadeParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeColorFadeActive, applyColorFadeActive},
    {"holdTime", "Hold Time", BLEEffectParamType::FLOAT, true, true, 0.0f, 20.0f, 0.1f, nullptr, 0, writeColorFadeHoldTime, applyColorFadeHoldTime},
    {"fadeTime", "Fade Time", BLEEffectParamType::FLOAT, true, true, 0.0f, 20.0f, 0.1f, nullptr, 0, writeColorFadeFadeTime, applyColorFadeFadeTime},
};

const BLEEffectParamDescriptor commitParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeCommitActive, applyCommitActive},
    {"commitSpeed", "Commit Speed", BLEEffectParamType::INTEGER, true, true, 0, 100000, 100, nullptr, 0, writeCommitSpeed, applyCommitSpeed},
    {"trailLength", "Trail Length", BLEEffectParamType::INTEGER, true, true, 0, 65535, 100, nullptr, 0, writeCommitTrailLength, applyCommitTrailLength},
    {"commitInterval", "Commit Interval", BLEEffectParamType::INTEGER, true, true, 0, 60000, 10, nullptr, 0, writeCommitInterval, applyCommitInterval},
    {"headColor", "Head Color", BLEEffectParamType::RGB, true, false, 0, 0, 0, nullptr, 0, writeCommitHeadColor, applyCommitHeadColor},
};

const BLEEffectParamDescriptor serviceLightsParams[] = {
    {"active", "Active", BLEEffectParamType::BOOLEAN, true, false, 0, 0, 0, nullptr, 0, writeServiceLightsActive, applyServiceLightsActive},
    {"mode", "Mode", BLEEffectParamType::ENUM, true, false, 0, 0, 0, serviceLightsModeOptions, countOf(serviceLightsModeOptions), writeServiceLightsMode, applyServiceLightsMode},
    {"color", "Color", BLEEffectParamType::RGB, true, false, 0, 0, 0, nullptr, 0, writeServiceLightsColor, applyServiceLightsColor},
    {"fastSpeed", "Fast Speed", BLEEffectParamType::FLOAT, true, true, 0.0f, 20.0f, 0.1f, nullptr, 0, writeServiceLightsFastSpeed, applyServiceLightsFastSpeed},
    {"slowSpeed", "Slow Speed", BLEEffectParamType::FLOAT, true, true, 0.0f, 20.0f, 0.1f, nullptr, 0, writeServiceLightsSlowSpeed, applyServiceLightsSlowSpeed},
    {"flashesPerCycle", "Flashes Per Cycle", BLEEffectParamType::INTEGER, true, true, 0, 100, 1, nullptr, 0, writeServiceLightsFlashes, applyServiceLightsFlashes},
};

const BLEEffectDescriptor effectDescriptors[] = {
    {"left_indicator", "Left Indicator", "signal", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT, leftIndicatorParams, countOf(leftIndicatorParams)},
    {"right_indicator", "Right Indicator", "signal", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT, rightIndicatorParams, countOf(rightIndicatorParams)},
    {"headlight", "Headlight", "core", BLE_STRIP_HEADLIGHT, headlightParams, countOf(headlightParams)},
    {"taillight", "Taillight", "core", BLE_STRIP_TAILLIGHT, taillightParams, countOf(taillightParams)},
    {"rgb", "RGB", "ambient", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT | BLE_STRIP_UNDERGLOW, rgbParams, countOf(rgbParams)},
    {"nightrider", "Night Rider", "ambient", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT | BLE_STRIP_UNDERGLOW, nightRiderParams, countOf(nightRiderParams)},
    {"police", "Police", "warning", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT | BLE_STRIP_UNDERGLOW, policeParams, countOf(policeParams)},
    {"pulse_wave", "Pulse Wave", "ambient", BLE_STRIP_HEADLIGHT | BLE_STRIP_UNDERGLOW, pulseWaveParams, countOf(pulseWaveParams)},
    {"aurora", "Aurora", "ambient", BLE_STRIP_UNDERGLOW, auroraParams, countOf(auroraParams)},
    {"solid_color", "Solid Color", "ambient", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT | BLE_STRIP_UNDERGLOW, solidColorParams, countOf(solidColorParams)},
    {"color_fade", "Color Fade", "ambient", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT | BLE_STRIP_UNDERGLOW, colorFadeParams, countOf(colorFadeParams)},
    {"commit", "Commit", "ambient", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT | BLE_STRIP_UNDERGLOW, commitParams, countOf(commitParams)},
    {"service_lights", "Service Lights", "warning", BLE_STRIP_HEADLIGHT | BLE_STRIP_TAILLIGHT | BLE_STRIP_UNDERGLOW, serviceLightsParams, countOf(serviceLightsParams)},
};

uint32_t fnv1a(const String &text)
{
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < text.length(); ++i)
  {
    hash ^= static_cast<uint8_t>(text[i]);
    hash *= 16777619UL;
  }
  return hash;
}
} // namespace

const BLEEffectDescriptor *BLEEffectsCatalog::getEffects(size_t &count)
{
  count = countOf(effectDescriptors);
  return effectDescriptors;
}

const BLEEffectDescriptor *BLEEffectsCatalog::findEffect(const char *effectId)
{
  if (effectId == nullptr)
  {
    return nullptr;
  }

  for (const BLEEffectDescriptor &effect : effectDescriptors)
  {
    if (strcmp(effect.id, effectId) == 0)
    {
      return &effect;
    }
  }

  return nullptr;
}

String BLEEffectsCatalog::buildCatalogJson()
{
  static String cachedCatalogJson;
  if (!cachedCatalogJson.isEmpty())
  {
    return cachedCatalogJson;
  }

  DynamicJsonDocument doc(16384);
  JsonObject root = doc.to<JsonObject>();
  root["protocolVersion"] = BLE_EFFECTS_PROTOCOL_VERSION;

  JsonArray effects = root.createNestedArray("effects");
  for (const BLEEffectDescriptor &effect : effectDescriptors)
  {
    JsonObject effectObject = effects.createNestedObject();
    effectObject["id"] = effect.id;
    effectObject["label"] = effect.label;
    effectObject["category"] = effect.category;

    JsonArray strips = effectObject.createNestedArray("supportedStrips");
    appendSupportedStrips(strips, effect.supportedStrips);

    JsonArray params = effectObject.createNestedArray("params");
    for (size_t i = 0; i < effect.paramCount; ++i)
    {
      const BLEEffectParamDescriptor &param = effect.params[i];
      JsonObject paramObject = params.createNestedObject();
      paramObject["key"] = param.key;
      paramObject["label"] = param.label;
      paramObject["writable"] = param.writable;

      switch (param.type)
      {
      case BLEEffectParamType::BOOLEAN:
        paramObject["type"] = "bool";
        break;
      case BLEEffectParamType::INTEGER:
        paramObject["type"] = "int";
        break;
      case BLEEffectParamType::FLOAT:
        paramObject["type"] = "float";
        break;
      case BLEEffectParamType::ENUM:
        paramObject["type"] = "enum";
        break;
      case BLEEffectParamType::RGB:
        paramObject["type"] = "rgb";
        break;
      }

      if (param.hasRange)
      {
        paramObject["min"] = param.minValue;
        paramObject["max"] = param.maxValue;
        paramObject["step"] = param.step;
      }

      if (param.optionCount > 0)
      {
        JsonArray options = paramObject.createNestedArray("options");
        for (size_t optionIndex = 0; optionIndex < param.optionCount; ++optionIndex)
        {
          JsonObject optionObject = options.createNestedObject();
          optionObject["id"] = param.options[optionIndex].id;
          optionObject["label"] = param.options[optionIndex].label;
        }
      }
    }
  }

  serializeJson(doc, cachedCatalogJson);
  return cachedCatalogJson;
}

String BLEEffectsCatalog::buildStateJson(Application &app)
{
  DynamicJsonDocument doc(12288);
  JsonObject root = doc.to<JsonObject>();
  root["protocolVersion"] = BLE_EFFECTS_PROTOCOL_VERSION;

  JsonObject effects = root.createNestedObject("effects");
  for (const BLEEffectDescriptor &effect : effectDescriptors)
  {
    JsonObject effectObject = effects.createNestedObject(effect.id);
    for (size_t i = 0; i < effect.paramCount; ++i)
    {
      effect.params[i].appendState(app, effectObject);
    }
  }

  String json;
  serializeJson(doc, json);
  return json;
}

bool BLEEffectsCatalog::applyCommand(Application &app, const String &commandJson, String &responseJson)
{
  DynamicJsonDocument requestDoc(4096);
  DeserializationError deserializationError = deserializeJson(requestDoc, commandJson);

  DynamicJsonDocument responseDoc(1024);
  JsonObject response = responseDoc.to<JsonObject>();
  response["ok"] = false;

  if (deserializationError)
  {
    response["error"] = "invalid json";
    serializeJson(responseDoc, responseJson);
    return false;
  }

  const char *effectId = requestDoc["effect"] | nullptr;
  JsonObjectConst params = requestDoc["params"].as<JsonObjectConst>();
  if (effectId == nullptr || params.isNull())
  {
    response["error"] = "expected effect and params";
    serializeJson(responseDoc, responseJson);
    return false;
  }

  const BLEEffectDescriptor *effect = findEffect(effectId);
  if (effect == nullptr)
  {
    response["error"] = "unknown effect";
    serializeJson(responseDoc, responseJson);
    return false;
  }

  size_t appliedCount = 0;
  for (JsonPairConst pair : params)
  {
    const BLEEffectParamDescriptor *paramDescriptor = nullptr;
    for (size_t i = 0; i < effect->paramCount; ++i)
    {
      if (strcmp(effect->params[i].key, pair.key().c_str()) == 0)
      {
        paramDescriptor = &effect->params[i];
        break;
      }
    }

    if (paramDescriptor == nullptr)
    {
      response["error"] = String("unknown param: ") + pair.key().c_str();
      serializeJson(responseDoc, responseJson);
      return false;
    }

    if (!paramDescriptor->writable)
    {
      response["error"] = String("param is read only: ") + pair.key().c_str();
      serializeJson(responseDoc, responseJson);
      return false;
    }

    String error;
    if (!paramDescriptor->applyValue(app, pair.value(), error))
    {
      response["error"] = error;
      serializeJson(responseDoc, responseJson);
      return false;
    }

    ++appliedCount;
  }

  response["ok"] = true;
  response["effect"] = effect->id;
  response["applied"] = appliedCount;
  serializeJson(responseDoc, responseJson);
  return true;
}

uint32_t BLEEffectsCatalog::getCatalogSchemaHash()
{
  static uint32_t cachedHash = 0;
  if (cachedHash == 0)
  {
    cachedHash = fnv1a(buildCatalogJson());
  }
  return cachedHash;
}

size_t BLEEffectsCatalog::getCatalogChunkCount()
{
  return getChunkCountForPayload(buildCatalogJson());
}

size_t BLEEffectsCatalog::getChunkCountForPayload(const String &payload)
{
  if (payload.isEmpty())
  {
    return 1;
  }

  return (payload.length() + BLE_EFFECTS_CHUNK_SIZE - 1) / BLE_EFFECTS_CHUNK_SIZE;
}

String BLEEffectsCatalog::getCatalogChunk(size_t chunkIndex)
{
  return getPayloadChunk(buildCatalogJson(), chunkIndex);
}

String BLEEffectsCatalog::getPayloadChunk(const String &payload, size_t chunkIndex)
{
  if (payload.isEmpty())
  {
    return "{}";
  }

  size_t start = chunkIndex * BLE_EFFECTS_CHUNK_SIZE;
  if (start >= payload.length())
  {
    return "";
  }

  size_t end = start + BLE_EFFECTS_CHUNK_SIZE;
  if (end > payload.length())
  {
    end = payload.length();
  }

  return payload.substring(start, end);
}
