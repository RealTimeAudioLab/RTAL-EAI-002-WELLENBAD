static inline const char* currentCategoryName() {
  if (currentProgramNumber < 30) return categoryNames[factoryCategory[currentProgramNumber]];
  return "USER";
}


const char* currentLiveWavetableName() {
  uint8_t t = selectedWavetableIndexAudio();
  int16_t sd = sdIndexForVisibleSlot(t);
  if (sd >= 0) return sdWavetableNames[sd];
  if (t < PPG_WT_TABLES) return PPG_WT_NAMES[t];
  return "WT?";
}


void rebuildWaveMonitorCache(uint8_t table, uint8_t logicalWave, uint8_t yMid, uint8_t height) {
  waveMonitorCacheTable = table;
  waveMonitorCacheWave = logicalWave;

  for (uint8_t x = 0; x < 128; x++) {
    uint8_t sampleIndex = x << 1; // 128 OLED pixels from 256 WT samples
    int16_t s = readPPGSample(table, logicalWave, sampleIndex);
    int y = (int)yMid - ((int32_t)s * (height / 2)) / 32768L;

    if (y < 24) y = 24;
    if (y > 62) y = 62;

    waveMonitorCacheY[x] = (int8_t)y;
  }

  waveMonitorCacheValid = true;
}


void invalidateWaveMonitorCache() {
  waveMonitorCacheValid = false;
}


void drawWaveformLine(uint8_t table, uint8_t logicalWave, uint8_t yMid, uint8_t height) {
  if (!waveMonitorCacheValid ||
      waveMonitorCacheTable != table ||
      waveMonitorCacheWave != logicalWave) {
    rebuildWaveMonitorCache(table, logicalWave, yMid, height);
  }

  int lastX = 0;
  int lastY = waveMonitorCacheY[0];

  for (uint8_t x = 1; x < 128; x++) {
    int y = waveMonitorCacheY[x];
    u8g2.drawLine(lastX, lastY, x, y);
    lastX = x;
    lastY = y;
  }
}


void drawWaveMonitorPage() {
  uint8_t baseTable = selectedWavetableIndex();
  uint8_t liveTable = selectedWavetableIndexAudio();

  uint8_t logicalWave = currentLogicalWaveIndex();
  uint16_t physicalWave = logicalToPhysicalWaveForDisplay(liveTable, logicalWave);
  uint16_t waveCount = currentPhysicalWaveCountForDisplay(liveTable);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.setCursor(0, 8);
  u8g2.print("WAVE MONITOR");

  u8g2.setCursor(0, 18);
  u8g2.print("WT ");
  u8g2.print(currentLiveWavetableName());

  u8g2.setCursor(98, 18);
  u8g2.print("#");
  u8g2.print(liveTable);

  u8g2.setCursor(0, 28);
  int16_t seqOff = (int16_t)liveTable - (int16_t)baseTable;
  if (getAParam(P_SEQ_TARGET) == P_WAVETABLE && getAParam(P_SEQ_MODE) != 0) {
    u8g2.print("SEQ ");
    u8g2.print(seqTableModeName(modSeqTableMode));
    u8g2.print(" ");
    u8g2.print(liveTable);
  } else {
    u8g2.print("SEQ OFF");
  }

  u8g2.setCursor(72, 28);
  u8g2.print("P");
  u8g2.print(physicalWave);
  u8g2.print("/");
  u8g2.print(waveCount - 1);

  u8g2.drawHLine(0, 44, 128);
  drawWaveformLine(liveTable, logicalWave, 44, 30);

  uint8_t bar = 0;
  if (waveMonitorTableSelect) {
    uint8_t total = totalWavetableCount();
    if (total < 2) total = 2;
    bar = ((uint16_t)liveTable * 127) / (total - 1);

    u8g2.setCursor(0, 58);
    u8g2.print("SEL TABLE ");
    u8g2.print(baseTable);
  } else {
    bar = ((uint16_t)logicalWave * 127) / 63;

    u8g2.setCursor(0, 58);
    u8g2.print("Wave ");
    u8g2.print(logicalWave);
    u8g2.print("/63");
  }

  u8g2.drawHLine(0, 61, 128);
  u8g2.drawBox(0, 59, bar, 3);
  u8g2.sendBuffer();
}


const char* lfoTargetName(uint8_t v) {
  switch (v) {
    case LFO_TO_WAVE: return "Wave";
    case LFO_TO_PITCH: return "Pitch";
    case LFO_TO_FILTER: return "Filter";
    case LFO_TO_AMP: return "Amp";
    case LFO_TO_PAN: return "Pan";
    default: return "?";
  }
}


const char* lfoShapeName(uint8_t v) {
  switch (v) {
    case 0: return "TRI";
    case 1: return "SINE";
    case 2: return "SAW UP";
    case 3: return "SAW DN";
    case 4: return "SQUARE";
    case 5: return "RANDOM";
    default: return "TRI";
  }
}


const char* playModeName(uint8_t v) {
  switch (v) {
    case 0: return "POLY";
    case 1: return "MONO";
    case 2: return "UNISON";
    case 3: return "POLY GLIDE";
    default: return "POLY";
  }
}


const char* clockName(uint8_t v) { return v ? "MIDI" : "INT"; }


const char* ccModeName(uint8_t v) {
  return v ? "LEARN" : "MOPHO";
}


const char* uiParamName(uint8_t p) {
  if (p == UI_PARAM_CC_MODE) return "CC Mode";
  if (p == UI_PARAM_SEQ_STEP) return "Seq Step";
  if (p == UI_PARAM_SEQ_VALUE) return "Seq Value";
  if (p == UI_PARAM_SEQ_TABLE_MODE) return "Tbl Mode";
  if (p == UI_PARAM_LFO_SHAPE) return "LFO Shape";
  if (p < PARAM_COUNT) return paramNames[p];
  return "";
}


uint8_t uiParamValue(uint8_t p) {
  if (p == UI_PARAM_CC_MODE) return midiCcMode ? 127 : 0;
  if (p == UI_PARAM_SEQ_STEP) {
    uint8_t maxStep = getAParam(P_SEQ_STEPS);
    if (maxStep < 1) maxStep = 1;
    if (maxStep > MODSEQ_STEPS) maxStep = MODSEQ_STEPS;
    if (modSeqEditStep >= maxStep) modSeqEditStep = maxStep - 1;
    return modSeqEditStep + 1;
  }
  if (p == UI_PARAM_SEQ_VALUE) return modSeqValues[modSeqEditStep];
  if (p == UI_PARAM_SEQ_TABLE_MODE) return modSeqTableMode ? 127 : 0;
  if (p == UI_PARAM_LFO_SHAPE) return globalLfoShape;
  if (p < PARAM_COUNT) return getAParam(p);
  return 0;
}


void printWtNumber3(uint8_t wt) {
  u8g2.print("WT");
  if (wt < 100) u8g2.print("0");
  if (wt < 10) u8g2.print("0");
  u8g2.print(wt);
}


const char* seqTableModeName(uint8_t v) {
  return v ? "REL" : "ABS";
}


const char* arpModeName(uint8_t v) {
  switch (v) {
    case 0: return "OFF";
    case 1: return "UP";
    case 2: return "DOWN";
    case 3: return "UPDN";
    case 4: return "RAND";
    default: return "?";
  }
}


const char* seqModeName(uint8_t v) {
  switch (v) {
    case 0: return "OFF";
    case 1: return "FORWARD";
    case 2: return "PENDUL";
    case 3: return "RANDOM";
    default: return "?";
  }
}


const char* seqRateName(uint8_t v) {
  switch (v) {
    case 0: return "1/4";
    case 1: return "1/8";
    case 2: return "1/8T";
    case 3: return "1/16";
    case 4: return "1/16T";
    case 5: return "1/32";
    default: return "?";
  }
}


const char* seqTargetName(uint8_t p) {
  switch (p) {
    case P_WAVETABLE: return "Table";
    case P_WAVE_POS: return "WavePos";
    case P_WAVE_MOD: return "WaveMod";
    case P_OSC_MIX: return "OscMix";
    case P_OSC_DETUNE: return "Detune";
    case P_OSC_B_OFFSET: return "OscBOff";
    case P_CUTOFF: return "Cutoff";
    case P_RESONANCE: return "Reso";
    case P_FILTER_ENV: return "FiltEnv";
    case P_ATTACK: return "Attack";
    case P_DECAY: return "Decay";
    case P_SUSTAIN: return "Sustain";
    case P_RELEASE: return "Release";
    case P_F_ATTACK: return "FAttack";
    case P_F_DECAY: return "FDecay";
    case P_F_SUSTAIN: return "FSustain";
    case P_F_RELEASE: return "FRelease";
    case P_WAVE_ENV: return "WaveEnv";
    case P_WAVE_ENV_ATTACK: return "WAttack";
    case P_WAVE_ENV_DECAY: return "WDecay";
    case P_WAVE_ENV_SUSTAIN: return "WSustain";
    case P_WAVE_ENV_RELEASE: return "WRelease";
    case P_LFO_RATE: return "LfoRate";
    case P_LFO_AMOUNT: return "LfoAmt";
    case P_LFO_TARGET: return "LfoTgt";
    case P_WAVE_LFO_RATE: return "WLfoRate";
    case P_WAVE_LFO_AMOUNT: return "WLfoAmt";
    case P_AFTERTOUCH_WAVE: return "ATWave";
    case P_AFTERTOUCH_FILTER: return "ATFilt";
    case P_VEL_AMP: return "VelAmp";
    case P_VEL_FILTER: return "VelFilt";
    case P_KEYTRACK: return "KeyTrk";
    case P_PAN_SPREAD: return "PanSprd";
    case P_DRIVE: return "Drive";
    case P_BITCRUSH: return "Bitcr";
    case P_VOLUME: return "Volume";
    case P_PLAY_MODE: return "Play";
    case P_GLIDE: return "Glide";
    case P_BEND_RANGE: return "Bend";
    case P_UNISON_DETUNE: return "UniDet";
    case P_SUB_LEVEL: return "Sub";
    case P_NOISE_LEVEL: return "Noise";
    case P_CHORUS: return "Chorus";
    case P_ARP_MODE: return "ArpMode";
    case P_ARP_RATE: return "ArpRate";
    case P_ARP_OCTAVES: return "ArpOct";
    case P_ARP_HOLD: return "ArpHold";
    case P_CLOCK_SOURCE: return "Clock";
    case P_TAP_TEMPO: return "Tempo";
    case P_MORPH_AMOUNT: return "Morph";
    default: return "WavePos";
  }
}


void updateDisplay() {
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw < 40) return;
  lastDraw = millis();

  if (userSaveMode) {
    drawUserSavePage();
    return;
  }

  if (presetBrowserActive) {
    drawPresetBrowser();
    return;
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 8); u8g2.print("WELLENBAD by RTA");
  u8g2.setCursor(100, 8); u8g2.print("P"); if (currentProgramNumber < 100) u8g2.print("0"); if (currentProgramNumber < 10) u8g2.print("0"); u8g2.print(currentProgramNumber);
  u8g2.setCursor(0, 18); u8g2.print(pageNames[uiPage]);
  u8g2.setCursor(62, 18); u8g2.print(currentProgram.name);
  if (compareMode) {
    u8g2.setCursor(0, 28);
    u8g2.print("COMPARE ORIG");
  }

  if (ccLearnMode) {
    u8g2.setCursor(0, 32); u8g2.print("CC LEARN:");
    u8g2.setCursor(0, 44); u8g2.print(paramNames[learnParam]);
    u8g2.setCursor(0, 56); u8g2.print("Move MIDI CC");
    u8g2.setCursor(0, 64); u8g2.print("Long Enc = Cancel");
    u8g2.sendBuffer(); return;
  }

  if (millis() < compareMessageUntil) {
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(0, 36);
    u8g2.print(compareMode ? "COMPARE: ORIGINAL" : "COMPARE: EDITED");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 54);
    u8g2.print("BTN3 long toggles");
    u8g2.sendBuffer();
    return;
  }

  if (millis() < programPreviewUntil && uiPage != PAGE_PROGRAM) {
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.setCursor(0, 34);
    u8g2.print("P"); if (currentProgramNumber < 100) u8g2.print("0"); if (currentProgramNumber < 10) u8g2.print("0"); u8g2.print(currentProgramNumber);
    u8g2.print(" "); u8g2.print(currentProgram.name);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 50); u8g2.print("["); u8g2.print(currentCategoryName()); u8g2.print("] WT "); u8g2.print(currentWavetableName());
    u8g2.sendBuffer(); return;
  }

  if (uiPage == PAGE_WAVE_MON) {
    drawWaveMonitorPage();
    return;
  }

  if (uiPage == PAGE_MORPH) {
    uint8_t amount = morphSource == 0 ? getAParam(P_MORPH_AMOUNT) : morphEffectiveAmount;

    if (millis() < morphCaptureMessageUntil) {
      u8g2.setFont(u8g2_font_7x13B_tf);
      u8g2.setCursor(0, 34);
      u8g2.print(morphCaptureMessage);
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(0, 54);
      u8g2.print("A/B snapshots updated");
      u8g2.sendBuffer();
      return;
    }

    u8g2.setCursor(0, 30);
    u8g2.print("Amount ");
    u8g2.print(amount);
    u8g2.print(" ");
    u8g2.print(morphSourceName(morphSource));

    u8g2.setCursor(0, 42);
    u8g2.print("A ");
    if (morphAValid) u8g2.print(morphNameA[0] ? morphNameA : "OK");
    else u8g2.print("--");

    u8g2.setCursor(0, 52);
    u8g2.print("B ");
    if (morphBValid) u8g2.print(morphNameB[0] ? morphNameB : "OK");
    else u8g2.print("--");

    u8g2.setCursor(0, 64);
    u8g2.print("B7=A B8=B B5/6 Src");
    u8g2.sendBuffer();
    return;
  }

  if (uiPage == PAGE_PROGRAM) {
    uint8_t qWaiting = audioEventQueue ? uxQueueMessagesWaiting(audioEventQueue) : 0;
    u8g2.setCursor(0, 30); u8g2.print(FW_VERSION); u8g2.print(" SD"); u8g2.print(sdWavetableCount); u8g2.print("/"); u8g2.print(sdCardOk ? "OK" : "--");
    u8g2.setCursor(0, 40); u8g2.print("VOI "); u8g2.print(activeVoicesLast); u8g2.print(" STL "); u8g2.print(voiceSteals); u8g2.print(" Q "); u8g2.print(qWaiting);
    u8g2.setCursor(0, 50); u8g2.print("PC "); u8g2.print(programChangeCount); u8g2.print(" PN "); u8g2.print(panicCount); u8g2.print(" Qd "); u8g2.print(audioQueueDrops);
    u8g2.setCursor(0, 60); u8g2.print("H "); u8g2.print(freeHeapLast/1024); u8g2.print("K P "); u8g2.print(freePsramLast/1024); u8g2.print("K");
    u8g2.sendBuffer(); return;
  }

  // Show a 4-line scrolling window. Some pages contain up to 6 parameters.
  uint8_t maxRows = countParamsOnPage(uiPage);
  uint8_t firstRow = 0;
  if (maxRows > 4 && uiRow >= 4) firstRow = uiRow - 3;

  for (int slot = 0; slot < 4; slot++) {
    uint8_t row = firstRow + slot;
    if (row >= maxRows) continue;

    uint8_t p = pageParams[uiPage][row];
    if (p == UI_PARAM_NONE) continue;

    int y = 30 + slot * 10;
    if (row == uiRow) { u8g2.setCursor(0, y); u8g2.print(">"); }

    // Parameter name + value spacing:
    // Draw the value a few pixels after the real text width of the name.
    // This avoids cramped output like "Cutoff92" or "Arp HoldON".
    u8g2.setCursor(8, y);
    u8g2.print(uiParamName(p));

    int valueX = 8 + u8g2.getStrWidth(uiParamName(p)) + 6;
    if (valueX < 44) valueX = 44;
    if (valueX > 92) valueX = 92;

    if (p == UI_PARAM_SEQ_TABLE_MODE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(seqTableModeName(modSeqTableMode));
    }
    else if (p == UI_PARAM_SEQ_STEP) {
      u8g2.setCursor(valueX, y);
      if (seqTargetIsTable()) {
        u8g2.print("Step ");
        u8g2.print(modSeqEditStep + 1);
        u8g2.print(" ");
        if (modSeqTableMode == 0) {
          printWtNumber3(modSeqValues[modSeqEditStep]);
        } else {
          uint16_t wt = (uint16_t)getAParam(P_WAVETABLE) + modSeqValues[modSeqEditStep];
          if (wt >= WT_VISIBLE_SLOTS) wt = WT_LAST_SLOT;
          printWtNumber3((uint8_t)wt);
        }
      } else {
        u8g2.print(modSeqEditStep + 1);
        u8g2.print("/");
        u8g2.print(getAParam(P_SEQ_STEPS));
      }
    }
    else if (p == UI_PARAM_SEQ_VALUE) {
      u8g2.setCursor(valueX, y);
      if (seqTargetIsTable()) {
        if (modSeqTableMode == 0) {
          printWtNumber3(modSeqValues[modSeqEditStep]);
        } else {
          u8g2.print("+");
          u8g2.print(modSeqValues[modSeqEditStep]);
        }
      } else {
        u8g2.print(modSeqValues[modSeqEditStep]);
      }
    }
    else if (p == UI_PARAM_CC_MODE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(ccModeName(midiCcMode));
    }
    else if (p == P_WAVETABLE) {
      uint8_t t = selectedWavetableIndex();
      u8g2.setCursor(valueX, y);
      u8g2.print(currentWavetableName());
      u8g2.setCursor(112, y);
      u8g2.print(t);
    }
    else if (p == P_LFO_TARGET) {
      u8g2.setCursor(valueX, y);
      u8g2.print(lfoTargetName(getAParam(p)));
    }
    else if (p == UI_PARAM_LFO_SHAPE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(lfoShapeName(globalLfoShape));
    }
    else if (p == P_PLAY_MODE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(playModeName(getAParam(p)));
    }
    else if (p == P_ARP_MODE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(arpModeName(getAParam(p)));
    }
    else if (p == P_ARP_RATE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(arpRateName(getAParam(p)));
    }
    else if (p == P_ARP_HOLD) {
      u8g2.setCursor(valueX, y);
      u8g2.print(switchIsOn(getAParam(p)) ? "ON" : "OFF");
    }
    else if (p == P_CLOCK_SOURCE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(clockName(getClockSource()));
    }
    else if (p == P_SEQ_MODE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(seqModeName(getAParam(p)));
    }
    else if (p == P_SEQ_RATE) {
      u8g2.setCursor(valueX, y);
      u8g2.print(seqRateName(getAParam(p)));
    }
    else if (p == P_SEQ_TARGET) {
      u8g2.setCursor(valueX, y);
      u8g2.print(seqTargetName(getAParam(p)));
    }
    else {
      u8g2.setCursor(valueX, y);
      u8g2.print(getAParam(p));
    }
  }

  if (storeMessage && millis() - storeMessageTime < 1200) {
    u8g2.setCursor(0, 64);
    u8g2.print("PROGRAM SAVED");
  } else if (uiPage == PAGE_MIDI) {

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(0, 54);
    u8g2.print("MIDI CC SOURCE ");
    u8g2.print(midiCcMode == 0 ? "MOPHO" : "LEARN"); 
    u8g2.print("  B");
    u8g2.print(midiCcBank);   

    u8g2.setCursor(0, 64);
    if (getClockSource()) {
      u8g2.print("EXT BPM ");
      u8g2.print(getMidiBpmForDisplay());
      u8g2.print(midiClockRunning ? "  RUN" : "  STOP");
      u8g2.print("  CH");
      u8g2.print(midiChannel);
    } else {
      u8g2.print("INT BPM ");


      u8g2.print(bpm);
      u8g2.print("  CH");
      u8g2.print(midiChannel);
    }
    u8g2.setFont(u8g2_font_6x10_tf); 

  }
  u8g2.sendBuffer();
}


void midiChannelBootMenu() {
  uint8_t ch = midiChannel;
  uint32_t lastDraw = 0;
  uint32_t lastStep = 0;
  drawMidiChannelBootScreen(ch);

  while (true) {
    uint32_t now = millis();

    if (now - lastDraw > 80) {
      lastDraw = now;
      drawMidiChannelBootScreen(ch);
    }

    if (now - lastStep > 180) {
      if (digitalRead(BTN_5_PIN) == LOW) {
        if (ch > 1) ch--;
        lastStep = now;
        drawMidiChannelBootScreen(ch);
      }

      if (digitalRead(BTN_6_PIN) == LOW) {
        if (ch < 16) ch++;
        lastStep = now;
        drawMidiChannelBootScreen(ch);
      }
    }

    if (digitalRead(BTN_8_PIN) == LOW) {
      midiChannel = ch;
      prefs.putUChar("midiCh", midiChannel);
      saveSystemConfigToSD();
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(0, 18);
      u8g2.print("MIDI CHANNEL SAVED");
      u8g2.setCursor(0, 38);
      u8g2.print("CHANNEL: ");
      u8g2.print(midiChannel);
      u8g2.sendBuffer();
      delay(600);
      return;
    }
    delay(5);
  }
}


void showSplashScreen() {
  u8g2.clearBuffer();
  // Outer frame
  u8g2.drawFrame(0, 0, 128, 64);
  // Brand
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(16, 11);
  u8g2.print("RealTimeAudioLab");
  // Main panel
  u8g2.drawFrame(8, 16, 112, 35);
  // Product title
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.setCursor(30, 28);
  u8g2.print("WELLENBAD");
  drawSplashWave();
  // Version footer
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(5, 61);
  u8g2.print("Wavetable Synth");
  u8g2.setCursor(89, 61);
  u8g2.print(FW_VERSION);
  u8g2.sendBuffer();
}
