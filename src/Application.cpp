// application.cpp

#include "Application.h"
#include "config.h"
#include "IO/LED/LEDStripManager.h"
#include "IO/StatusLed.h"
#include "IO/TimeProfiler.h"
#include <Wireless.h>
#include <math.h>

//----------------------------------------------------------------------------
namespace
{
constexpr uint16_t EFFECT_SYNC_PROPERTY_KEY = 0x0F10;
}

Application *Application::getInstance()
{
  static Application instance;
  return &instance;
}

//----------------------------------------------------------------------------
/*
 * Application constructor. Initialize pointers and set defaults.
 */
Application::Application()
{
// Initialize HVInput instances.
#ifdef ENABLE_HV_INPUTS
  accOnInput = HVInput(&input1, HV_HIGH_ACTIVE);
  leftIndicatorInput = HVInput(&input2, HV_HIGH_ACTIVE);
  rightIndicatorInput = HVInput(&input3, HV_HIGH_ACTIVE);

  headlightInput = HVInput(&input4, HV_HIGH_ACTIVE);

  brakeInput = HVInput(&input5, HV_HIGH_ACTIVE);
  reverseInput = HVInput(&input6, HV_HIGH_ACTIVE);

#endif

  appInitialized = false;
}

/*
 * Begin: Initializes the LED strip, effects, wireless, etc.
 */
void Application::begin()
{
  if (appInitialized)
    return;

  appInitialized = true;

  mode = static_cast<ApplicationMode>(preferences.getUInt("mode", 0));
  prevMode = mode == ApplicationMode::NORMAL ? ApplicationMode::OFF : ApplicationMode::NORMAL;

  LEDStripManager *ledManager = LEDStripManager::getInstance();
  ledManager->begin();

  ledManager->setBrightness(255);

  ledManager->startTask();

  if (ledConfig.headlightsEnabled)
  {
    LEDStripConfig headlights(
        LEDStripType::HEADLIGHT,
        // new LEDStrip(ledConfig.headlightLedCount, OUTPUT_LED_1_PIN),
        "Headlights",
        ledConfig.headlightLedCount,
        OUTPUT_LED_1_PIN);
    headlights.strip->setFliped(ledConfig.headlightFlipped);

    ledManager->addLEDStrip(headlights);
    headlights.strip->setActive(true); // default to active so that the strip is visible when the app is started

    headlights.strip->getBuffer()[0] = Color(255, 0, 0);
    delay(500);
    headlights.strip->getBuffer()[0] = Color(0, 0, 0);
  }

  if (ledConfig.taillightsEnabled)
  {
    LEDStripConfig taillights(
        LEDStripType::TAILLIGHT,
        // new LEDStrip(ledConfig.taillightLedCount, OUTPUT_LED_2_PIN),
        "Taillights",
        ledConfig.taillightLedCount,
        OUTPUT_LED_2_PIN);
    taillights.strip->setFliped(ledConfig.taillightFlipped);

    ledManager->addLEDStrip(taillights);
    taillights.strip->setActive(true); // default to active so that the strip is visible when the app is started

    taillights.strip->getBuffer()[0] = Color(255, 0, 0);
    delay(500);
    taillights.strip->getBuffer()[0] = Color(0, 0, 0);
  }

  if (ledConfig.underglowEnabled)
  {
    LEDStripConfig underglow(
        LEDStripType::UNDERGLOW,
        // new LEDStrip(ledConfig.underglowLedCount, OUTPUT_LED_3_PIN),
        "Underglow",
        ledConfig.underglowLedCount,
        OUTPUT_LED_3_PIN);
    LEDSegment *underglowSegmentL = new LEDSegment(underglow.strip, "Underglow-Left", 0, 102);    // 0 - 101
    LEDSegment *underglowSegmentF = new LEDSegment(underglow.strip, "Underglow-Front", 102, 96);  // 102 - 197
    LEDSegment *underglowSegmentR = new LEDSegment(underglow.strip, "Underglow-Right", 198, 102); // 198 - 299

    underglow.strip->setFliped(ledConfig.underglowFlipped);

    ledManager->addLEDStrip(underglow);
    underglow.strip->setActive(false); // default to inactive so that the strip is not visible when the app is started

    underglow.strip->getBuffer()[0] = Color(255, 0, 0);
    delay(500);
    underglow.strip->getBuffer()[0] = Color(0, 0, 0);

    // underglow.strip->getBuffer()[101] = Color(255, 0, 0);

    // underglow.strip->getBuffer()[197] = Color(255, 0, 0);

    // underglow.strip->getBuffer()[299] = Color(255, 0, 0);
    // underglow.strip->getBuffer()[300] = Color(0, 0, 255);

    // for (int i = 0; i < underglowSegmentL->getNumLEDs(); i++) // quick test segments
    //   underglowSegmentL->getBuffer()[i] = Color(255, 0, 0);
    // for (int i = 0; i < underglowSegmentF->getNumLEDs(); i++)
    //   underglowSegmentF->getBuffer()[i] = Color(0, 255, 0);
    // for (int i = 0; i < underglowSegmentR->getNumLEDs(); i++)
    //   underglowSegmentR->getBuffer()[i] = Color(0, 0, 255);
    // while (true)
    //   ;
  }

  if (ledConfig.interiorEnabled)
  {
    LEDStripConfig interior(
        LEDStripType::INTERIOR,
        // new LEDStrip(ledConfig.interiorLedCount, OUTPUT_LED_4_PIN),
        "Interior",
        ledConfig.interiorLedCount,
        OUTPUT_LED_4_PIN);
    interior.strip->setFliped(ledConfig.interiorFlipped);

    ledManager->addLEDStrip(interior);
    interior.strip->setActive(false); // default to inactive so that the strip is not visible when the app is started

    interior.strip->getBuffer()[0] = Color(255, 0, 0);
    delay(500);
    interior.strip->getBuffer()[0] = Color(0, 0, 0);
  }

  setupEffects();
  setupSequences();
  setupWireless();
  setupMesh();
  setupBLE();

  for (auto &pair : ledManager->getStrips())
  {
    Serial.println("Strip: " + pair.second.name);
    Serial.println(" | Num LEDs: " + String(pair.second.strip->getNumLEDs()));
    Serial.println(" | Type: " + String(static_cast<int>(pair.second.strip->getType())));
    Serial.println(" | Enabled: " + String(pair.second.strip->getEnabled() ? "true" : "false"));
    Serial.println(" | Active: " + String(pair.second.strip->getActive() ? "true" : "false"));
    Serial.println(" | Fliped: " + String(pair.second.strip->getFliped() ? "true" : "false"));
    Serial.println(" | Brightness: " + String(pair.second.strip->getBrightness()));

    for (auto &segment : pair.second.strip->getSegments())
    {
      Serial.println(" | Segment: " + segment->name);
      Serial.println("    | Num LEDs: " + String(segment->getNumLEDs()));
      Serial.println("    | Enabled: " + String(segment->getEnabled() ? "true" : "false"));
      Serial.println("    | Fliped: " + String(segment->fliped ? "true" : "false"));
      Serial.println("    | Start Index: " + String(segment->startIndex));
      Serial.println("    | Parent Strip: " + segment->getParentStrip()->getName());
      Serial.println("    | Effects: " + String(segment->effectCount()));
    }
  }

  // unlockSequence->trigger(); // car is most likely unlocked when device is powered on
  unlockSequence->setActive(false);

  // serviceLightsEffect->setActive(true);
  // serviceLightsEffect->setMode(ServiceLightsMode::STROBE);
}

ApplicationMode Application::getMode()
{
  return mode;
}

void Application::updateInputs()
{
  accOnInput.update();
  leftIndicatorInput.update();
  rightIndicatorInput.update();
  headlightInput.update();
  brakeInput.update();
  reverseInput.update();
}

/*
 * update():
 * Main loop update.
 */

static uint32_t lastUpdateInputs = 0;

void Application::loop()
{
  if (!appInitialized)
    return;

  timeProfiler.increment("appFps");
  timeProfiler.start("appLoop", TimeUnit::MICROSECONDS);
  // Update input states.
  // if (millis() - lastUpdateInputs > 20)
  // {
    timeProfiler.start("updateInputs", TimeUnit::MICROSECONDS);
    // lastUpdateInputs = millis();
    updateInputs();
    timeProfiler.stop("updateInputs");
  // }

  // handle remote dissconnection
  if (lastRemotePing != 0 && millis() - lastRemotePing > 2000)
  {
    if (mode == ApplicationMode::TEST || mode == ApplicationMode::REMOTE)
    {
      lastRemotePing = 0;
      mode = ApplicationMode::NORMAL;

      // disable all effects
      LEDEffect::disableAllEffects();
    }
  }

  // handle brake tap sequence. always check if this is triggered
  brakeTapSequence3->setInput(brakeInput.get());
  brakeTapSequence3->loop();

  timeProfiler.start("updateMode", TimeUnit::MICROSECONDS);
  switch (mode)
  {
  case ApplicationMode::NORMAL:
    handleNormalEffects();
    break;

  case ApplicationMode::TEST:
  {
    // handleTestEffects();
    handleNormalEffects();
  }
  break;

  case ApplicationMode::REMOTE:
    handleRemoteEffects();
    break;

  case ApplicationMode::OFF:
  {
    // turn off all effects
    LEDEffect::disableAllEffects();
    SyncManager::getInstance()->setSyncMode(SyncMode::SOLO);
  }
  break;
  }

  if (mode != prevMode)
  {
    prevMode = mode;
  }

  timeProfiler.stop("updateMode");

  // Update SyncManager
  timeProfiler.start("updateSync", TimeUnit::MICROSECONDS);
  SyncManager *syncMgr = SyncManager::getInstance();
  syncMgr->loop();
  updateSyncStatusLed();

  timeProfiler.stop("updateSync");

  // Update BLE
  timeProfiler.start("updateBLE", TimeUnit::MICROSECONDS);
  BLEManager *bleManager = BLEManager::getInstance();
  bleManager->loop();
  timeProfiler.stop("updateBLE");

  timeProfiler.start("updateEffects", TimeUnit::MICROSECONDS);
  LEDStripManager::getInstance()->updateEffects();
  timeProfiler.stop("updateEffects");

  timeProfiler.stop("appLoop");
}

void Application::enableNormalMode()
{
  mode = ApplicationMode::NORMAL;
  preferences.putUInt("mode", static_cast<int>(mode));

  // clear all overrides
  accOnInput.clearOverride();
  leftIndicatorInput.clearOverride();
  rightIndicatorInput.clearOverride();
  headlightInput.clearOverride();
  brakeInput.clearOverride();
  reverseInput.clearOverride();
}

void Application::enableTestMode()
{
  mode = ApplicationMode::TEST;
  preferences.putUInt("mode", static_cast<uint8_t>(mode));
}

void Application::enableRemoteMode()
{
  mode = ApplicationMode::REMOTE;
  preferences.putUInt("mode", static_cast<uint8_t>(mode));
}

void Application::enableOffMode()
{
  mode = ApplicationMode::OFF;
  preferences.putUInt("mode", static_cast<uint8_t>(mode));

  // turn off all effects
  LEDEffect::disableAllEffects();
}

void Application::handleTestEffects()
{
}

void Application::handleRemoteEffects()
{
  SyncManager *syncMgr = SyncManager::getInstance();
  bool isSyncing = syncMgr->isInGroup() && syncMgr->getGroupInfo().members.size() > 1;
  bool isMaster = syncMgr->isGroupMaster();

  if (isMaster && isEffectSyncEnabled())
  {
    EffectSyncState effectState = {};

    effectState.rgbSyncData = rgbEffect->getSyncData();
    effectState.nightRiderSyncData = nightriderEffect->getSyncData();
    effectState.policeSyncData = policeEffect->getSyncData();
    effectState.solidColorSyncData = solidColorEffect->getSyncData();
    effectState.colorFadeSyncData = colorFadeEffect->getSyncData();
    effectState.commitSyncData = commitEffect->getSyncData();

    setEffectSyncState(effectState);
  }
}

void Application::handleSyncedEffects(const EffectSyncState &effectState)
{
  rgbEffect->setSyncData(effectState.rgbSyncData);
  nightriderEffect->setSyncData(effectState.nightRiderSyncData);
  policeEffect->setSyncData(effectState.policeSyncData);
  solidColorEffect->setSyncData(effectState.solidColorSyncData);
  colorFadeEffect->setSyncData(effectState.colorFadeSyncData);
  commitEffect->setSyncData(effectState.commitSyncData);
  serviceLightsEffect->setSyncData(effectState.serviceLightsSyncData);
}

void Application::setupBLE()
{
  Serial.println("Application: Setting up BLE...");

  BLEManager *bleManager = BLEManager::getInstance();
  bleManager->setApplication(this);
  bleManager->begin();

  Serial.println("Application: BLE setup complete");
}

void Application::setupMesh()
{
  SyncManager *syncMgr = SyncManager::getInstance();
  syncMgr->setTransport(Wireless::getInstance());
  syncMgr->setModePersistence(
      []() -> uint8_t
      { return preferences.getUChar("sync_mode", static_cast<uint8_t>(SyncMode::SOLO)); },
      [](uint8_t mode)
      { preferences.putUChar("sync_mode", mode); });
  syncMgr->setDeviceIdProvider([]() -> uint32_t
                               { return deviceInfo.serialNumber != 0 ? deviceInfo.serialNumber : 0; });

  setupEffectSync();
  syncMgr->begin();

#ifdef ENABLE_SYNC
  syncMgr->setDeviceDiscoveredCallback([this](const DiscoveredDevice &device)
                                       { Serial.println("Application: Device discovered - ID: 0x" + String(device.deviceId, HEX)); });

  syncMgr->setGroupFoundCallback([this](const GroupAdvert &advert)
                                 { Serial.println("Application: Group found - ID: 0x" + String(advert.groupId, HEX)); });

  syncMgr->setGroupCreatedCallback([this](const GroupInfo &group)
                                   { Serial.println("Application: Group created - ID: 0x" + String(group.groupId, HEX)); });

  syncMgr->setGroupJoinedCallback([this](const GroupInfo &group)
                                  { Serial.println("Application: Joined group - ID: 0x" + String(group.groupId, HEX)); });

  syncMgr->setGroupLeftCallback([this]()
                                { Serial.println("Application: Left group"); });

  syncMgr->setTimeSyncCallback([this](uint32_t syncedTime)
                               {
                                 Serial.println("Application: Time synchronized - synced time: " + String(syncedTime));
                                 // TODO: Use synchronized time for effect timing coordination
                               });
#endif
}

void Application::setupEffectSync()
{
  effectSyncProperty = SyncManager::getInstance()->property<EffectSyncState>(EFFECT_SYNC_PROPERTY_KEY);
  effectSyncProperty.onChange([this](uint32_t deviceId, const EffectSyncState &effectState)
                              {
                                SyncManager *syncMgr = SyncManager::getInstance();
                                if (!syncMgr->isInGroup() || syncMgr->isGroupMaster() || deviceId == syncMgr->getDeviceId())
                                {
                                  return;
                                }

                                Serial.println("Application: Effect sync received");
                                handleSyncedEffects(effectState);
                              });
}

void Application::setEffectSyncState(const EffectSyncState &effectState)
{
  SyncManager *syncMgr = SyncManager::getInstance();
  if (!effectSyncEnabled || !syncMgr->isInGroup() || !syncMgr->isGroupMaster())
  {
    return;
  }

  effectSyncProperty.set(effectState);
}

bool Application::isEffectSyncEnabled() const
{
  return effectSyncEnabled;
}

void Application::updateSyncStatusLed()
{
  SyncManager *syncMgr = SyncManager::getInstance();
  uint32_t currentTime = millis();
  bool fastBlink = (currentTime % 500) < 250;
  bool slowBlink = (currentTime % 1000) < 500;
  bool verySlowBlink = (currentTime % 2000) < 1000;

  if (getMode() == ApplicationMode::OFF)
  {
    statusLed2.setColor(0, 0, 0);
    return;
  }

  if (syncMgr->isTimeSynced() && syncMgr->isInGroup())
  {
    uint32_t syncTime = syncMgr->getSyncedTime();
    slowBlink = (syncTime % 1000) < 500;
  }

  switch (syncMgr->getSyncMode())
  {
  case SyncMode::SOLO:
    statusLed2.setColor(100, 100, 100);
    break;
  case SyncMode::JOIN:
    if (!syncMgr->isInGroup())
    {
      statusLed2.setColor(fastBlink ? 255 : 0, fastBlink ? 255 : 0, 0);
    }
    else if (!syncMgr->isTimeSynced())
    {
      statusLed2.setColor(fastBlink ? 255 : 0, fastBlink ? 165 : 0, 0);
    }
    else
    {
      statusLed2.setColor(0, 0, slowBlink ? 255 : 0);
    }
    break;
  case SyncMode::HOST:
    if (!syncMgr->isInGroup())
    {
      statusLed2.setColor(verySlowBlink ? 255 : 0, 0, 0);
    }
    else if (syncMgr->isGroupMaster())
    {
      size_t memberCount = syncMgr->getGroupInfo().members.size();
      if (memberCount < 2)
      {
        statusLed2.setColor(255, 0, 255);
      }
      else if (syncMgr->isTimeSynced())
      {
        statusLed2.setColor(slowBlink ? 255 : 0, 0, slowBlink ? 255 : 0);
      }
      else
      {
        statusLed2.setColor(fastBlink ? 255 : 0, 0, fastBlink ? 255 : 0);
      }
    }
    else
    {
      statusLed2.setColor(0, fastBlink ? 255 : 0, fastBlink ? 255 : 0);
    }
    break;
  case SyncMode::AUTO:
    statusLed2.setColor(slowBlink ? 0 : 0, slowBlink ? 255 : 0, slowBlink ? 128 : 0);
    break;
  default:
    statusLed2.setColor(fastBlink ? 255 : 0, 0, 0);
    break;
  }
}
