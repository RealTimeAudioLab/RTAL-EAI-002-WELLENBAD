static inline int16_t sdIndexForVisibleSlot(uint8_t table) {
  uint8_t n = sdOverrideCount();
  if (n == 0) return -1;

  // Override region is the last n visible slots.
  // Example n=3: slots 124,125,126 are overridden.
  uint8_t firstOverrideSlot = WT_VISIBLE_SLOTS - n;
  if (table < firstOverrideSlot || table >= WT_VISIBLE_SLOTS) return -1;

  // User-requested reverse mapping:
  // SD0 -> 126, SD1 -> 125, SD2 -> 124 ...
  int16_t sd = (int16_t)WT_LAST_SLOT - (int16_t)table;
  if (sd < 0 || sd >= sdWavetableCount) return -1;
  return sd;
}


static inline uint8_t totalWavetableCount() {
  // The UI always sees exactly 127 slots: 0..126.
  return WT_VISIBLE_SLOTS;
}


static inline uint8_t selectedWavetableIndex() {
  uint8_t v = getAParam(P_WAVETABLE);
  if (v >= WT_VISIBLE_SLOTS) v = WT_LAST_SLOT;
  return v;
}


static inline uint8_t selectedWavetableIndexAudio() {
  // Audio path uses the non-destructive sequencer mirror so P_WAVETABLE
  // can also be modulated without changing the stored parameter.
  uint8_t v = getAParamSeq(P_WAVETABLE);
  if (v >= WT_VISIBLE_SLOTS) v = WT_LAST_SLOT;
  return v;
}


static inline uint8_t currentLogicalWaveIndex() {
  return (uint8_t)(((uint16_t)getAParam(P_WAVE_POS) * (PPG_WT_WAVES - 1) + 63) / 127);
}


static inline uint16_t currentPhysicalWaveCountForDisplay(uint8_t table) {
  int16_t sd = sdIndexForVisibleSlot(table);
  if (sd >= 0 && sd < sdWavetableCount && sdWavetableWaves[sd] > 0) return sdWavetableWaves[sd];
  return PPG_WT_WAVES;
}


static inline uint16_t logicalToPhysicalWaveForDisplay(uint8_t table, uint8_t logicalWave) {
  uint16_t waves = currentPhysicalWaveCountForDisplay(table);
  if (waves == 128) return ((uint16_t)logicalWave * 127UL + 31UL) / 63UL;
  if (logicalWave >= waves) return waves - 1;
  return logicalWave;
}


void requestSdWavetableCache(uint8_t sd) {
  if (sd >= sdWavetableCount) return;

  // Already active: no request needed.
  if (sdWtCacheReady && activeSdWtSlot == sd) return;

  requestedSdWtSlot = sd;
  sdWtCacheRequestCount++;
}


void processSdWavetableCacheRequest() {
  int16_t sd = requestedSdWtSlot;
  if (sd < 0) return;

  // Claim request. If another request arrives during loading, setParam()
  // will write requestedSdWtSlot again and it will be handled next cycle.
  requestedSdWtSlot = -1;

  if (sd >= sdWavetableCount) return;

  if (ensureSdWavetableCached((uint8_t)sd)) {
    sdWtCacheLoadCount++;
  } else {
    sdWtCacheLoadFailCount++;
  }
}


void loadSdWavetables() {
  freeSdWavetables();
  sdCardOk = false;
  pinMode(OLED_CS, OUTPUT);
  digitalWrite(OLED_CS, HIGH);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  if (!SD.begin(SD_CS_PIN, SPI, 10000000, "/sd", 5)) {
    Serial.println("SD not available - using internal wavetables only");
    return;
  }

  sdCardOk = true;
  allocateSdWtCaches();

  File dir = SD.open(SD_WT_DIR);
  if (!dir || !dir.isDirectory()) {
    Serial.println("No /PPGWT directory on SD - using internal wavetables only");
    if (dir) dir.close();
    return;
  }

  while (sdWavetableCount < SD_WT_MAX_TABLES) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      const char *name = entry.name();
      if (hasWtExtension(name)) {
        char path[96];

        // Some SD libraries return full path, some only basename.
        if (name[0] == '/') snprintf(path, sizeof(path), "%s", name);
        else snprintf(path, sizeof(path), "%s/%s", SD_WT_DIR, name);

        entry.close();

        if (loadSdWavetableFile(path, sdWavetableCount)) {
          sdWavetableCount++;
        }
        continue;
      }
    }
    entry.close();
  }
  dir.close();
  Serial.print("SD wavetable count: ");
  Serial.println(sdWavetableCount);
}


void ensureSdDirectories() {
  if (!sdCardOk) return;
  if (!SD.exists(SD_WT_DIR)) SD.mkdir(SD_WT_DIR);
  if (!SD.exists(SD_PRESET_DIR)) SD.mkdir(SD_PRESET_DIR);
  if (!SD.exists(SD_BACKUP_DIR)) SD.mkdir(SD_BACKUP_DIR);
  if (!SD.exists(SD_META_DIR)) SD.mkdir(SD_META_DIR);
}

bool saveSystemConfigToSD() {
  if (!sdCardOk) return false;
  ensureSdDirectories();

  if (SD.exists(SYS_CONFIG_FILE)) {
    SD.remove(SYS_CONFIG_FILE);
  }

  File f = SD.open(SYS_CONFIG_FILE, FILE_WRITE);
  if (!f) return false;

  f.seek(0);
  f.print("MIDI_CH=");
  f.println(midiChannel);
  f.print("CLOCK_SRC=");
  f.println(globalClockSource ? 1 : 0);
  f.print("CC_MODE=");
  f.println(midiCcMode ? 1 : 0);
  f.print("CC_BANK=");
  f.println(midiCcBank);
  f.print("MORPH_SRC=");
  f.println(morphSource);
  f.close();

  Serial.print("System config saved TXT: ch=");
  Serial.print(midiChannel);
  Serial.print(" clock=");
  Serial.print(globalClockSource ? "MIDI" : "INT");
  Serial.print(" ccMode=");
  Serial.print(midiCcMode);
  Serial.print(" ccBank=");
  Serial.print(midiCcBank);
  Serial.print(" morphSrc=");
  Serial.println(morphSource);

  return true;
}

bool loadSystemConfigFromSD() {
  if (!sdCardOk) return false;
  if (!SD.exists(SYS_CONFIG_FILE)) return false;

  File f = SD.open(SYS_CONFIG_FILE, FILE_READ);
  if (!f) return false;

  bool gotAny = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    int eq = line.indexOf('=');
    if (eq <= 0) continue;

    String key = line.substring(0, eq);
    int value = line.substring(eq + 1).toInt();

    if (key == "MIDI_CH") {
      midiChannel = (uint8_t)value;
      gotAny = true;
    } else if (key == "CLOCK_SRC") {
      globalClockSource = value ? 1 : 0;
      gotAny = true;
    } else if (key == "CC_MODE") {
      midiCcMode = value ? 1 : 0;
      gotAny = true;
    } else if (key == "CC_BANK") {
      midiCcBank = (uint8_t)value;
      gotAny = true;
    } else if (key == "MORPH_SRC") {
      morphSource = (uint8_t)value;
      gotAny = true;
    }
  }
  f.close();

  if (!gotAny) return false;

  if (midiChannel < 1 || midiChannel > 16) midiChannel = 1;
  if (globalClockSource > 1) globalClockSource = 0;
  if (midiCcMode > 1) midiCcMode = 0;
  if (midiCcBank > 2) midiCcBank = 0;
  if (morphSource > 2) morphSource = 0;

  Serial.print("System config loaded TXT: ch=");
  Serial.print(midiChannel);
  Serial.print(" clock=");
  Serial.print(globalClockSource ? "MIDI" : "INT");
  Serial.print(" ccMode=");
  Serial.print(midiCcMode);
  Serial.print(" ccBank=");
  Serial.print(midiCcBank);
  Serial.print(" morphSrc=");
  Serial.println(morphSource);

  return true;
}



bool loadFirstPresetFromSD() {
  if (!sdCardOk) return false;
  File dir = SD.open(SD_PRESET_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    sdPresetLoadErrors++;
    return false;
  }

  bool loaded = false;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      const char *n = entry.name();
      size_t l = strlen(n);
      if (l >= 4 && strcasecmp(n + l - 4, ".PPG") == 0) {
        char path[96];
        if (n[0] == '/') snprintf(path, sizeof(path), "%s", n);
        else snprintf(path, sizeof(path), "%s/%s", SD_PRESET_DIR, n);
        entry.close();
        loaded = loadPresetFromSD(path);
        break;
      }
    }
    entry.close();
  }
  dir.close();
  return loaded;
}


bool loadFirstBankFromSD() {
  if (!sdCardOk) return false;
  File dir = SD.open(SD_BACKUP_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    sdBankLoadErrors++;
    return false;
  }

  bool loaded = false;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      const char *n = entry.name();
      size_t l = strlen(n);
      if (l >= 4 && strcasecmp(n + l - 4, ".PBK") == 0) {
        char path[96];
        if (n[0] == '/') snprintf(path, sizeof(path), "%s", n);
        else snprintf(path, sizeof(path), "%s/%s", SD_BACKUP_DIR, n);
        entry.close();
        loaded = loadBankFromSD(path);
        break;
      }
    }
    entry.close();
  }
  dir.close();
  return loaded;
}
