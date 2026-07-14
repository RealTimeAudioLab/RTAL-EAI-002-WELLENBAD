// Buttons
BfButton btn1(BfButton::STANDALONE_DIGITAL, BTN_1_PIN, true, LOW);
BfButton btn2(BfButton::STANDALONE_DIGITAL, BTN_2_PIN, true, LOW);
BfButton btn3(BfButton::STANDALONE_DIGITAL, BTN_3_PIN, true, LOW);
BfButton btn4(BfButton::STANDALONE_DIGITAL, BTN_4_PIN, true, LOW);
BfButton btn5(BfButton::STANDALONE_DIGITAL, BTN_5_PIN, true, LOW);
BfButton btn6(BfButton::STANDALONE_DIGITAL, BTN_6_PIN, true, LOW);
BfButton btn7(BfButton::STANDALONE_DIGITAL, BTN_7_PIN, true, LOW);
BfButton btn8(BfButton::STANDALONE_DIGITAL, BTN_8_PIN, true, LOW);
int lastEncA = HIGH;

// ================================================================
// Buttons and encoder
// ================================================================
void buttonHandler(BfButton *btn, BfButton::press_pattern_t pattern) {
  if (pattern == BfButton::LONG_PRESS && btn == &btn1) { resetDiagnostics(); return; }
  if (pattern == BfButton::LONG_PRESS && btn == &btn2) { allNotesOff(); return; }
  if (pattern == BfButton::LONG_PRESS && btn == &btn7) {
    if (uiPage == PAGE_MIDI) tapTempo();
    else openPresetBrowser();
    return;
  }
  if (pattern == BfButton::LONG_PRESS && btn == &btn8) { captureMorphA(); return; }
  if (pattern == BfButton::LONG_PRESS && btn == &btn6) { captureMorphB(); return; }
  if (pattern == BfButton::LONG_PRESS && btn == &btn5) { randomizeMusicalProgram(); return; }
  if (pattern == BfButton::LONG_PRESS && btn == &btn4) { initCurrentProgram(); return; }
  if (pattern == BfButton::LONG_PRESS && btn == &btn3) { toggleCompareMode(); return; }

  // Morph page early button handling.
  // Must be before normal BTN7=next program and BTN8=save handling.
  if (uiPage == PAGE_MORPH && pattern == BfButton::SINGLE_PRESS) {
    if (btn == &btn7) { captureMorphA(); return; }
    if (btn == &btn8) { captureMorphB(); return; }
    if (btn == &btn5) { stepMorphSource(-1); return; }
    if (btn == &btn6) { stepMorphSource(1); return; }
  }

  if (userSaveMode) {
    if (pattern != BfButton::SINGLE_PRESS) return;

    if (userSaveConfirmOverwrite) {
      if (btn == &btn8) { saveCurrentProgramToUserSlot(); return; }
      if (btn == &btn2) { clearUserSaveOverwriteConfirm(); return; }
      return;
    }

    if (userNameEditMode) {
      if (btn == &btn1) { deleteNameChar(); return; }
      if (btn == &btn7) { clearProgramName(); return; }
      if (btn == &btn5) { stepNameChar(-1); return; }
      if (btn == &btn6) { stepNameChar(1); return; }
      if (btn == &btn3) { moveNameCursor(-1); return; }
      if (btn == &btn4) { moveNameCursor(1); return; }
      if (btn == &btn8) { finishUserNameEdit(); return; }
      if (btn == &btn2) { cancelUserNameEdit(); return; }
      return;
    }

    if (btn == &btn8) { saveCurrentProgramToUserSlot(); return; }
    if (btn == &btn2) { closeUserSaveMode(); return; }
    if (btn == &btn1) { startUserNameEdit(); return; }
    if (btn == &btn5 || btn == &btn3) { moveUserSaveSlot(-1); return; }
    if (btn == &btn6 || btn == &btn4) { moveUserSaveSlot(1); return; }
    return;
  }
  if (presetBrowserActive) {
    if (pattern != BfButton::SINGLE_PRESS) return;

    if (btn == &btn1) { browserMoveCategory(-1); return; }
    if (btn == &btn2) { browserMoveCategory(1); return; }
    if (btn == &btn3) { browserMoveProgram(-1); return; }
    if (btn == &btn4) { browserMoveProgram(1); return; }
    if (btn == &btn5) { browserMoveProgram(-1); return; }
    if (btn == &btn6) { browserMoveProgram(1); return; }
    if (btn == &btn7) { loadBrowserProgram(); return; }
    if (btn == &btn8) { closePresetBrowser(); return; }
    return;
  }

  if (pattern != BfButton::SINGLE_PRESS) return;

  if (btn == &btn1) { uiPage = uiPage > 0 ? uiPage - 1 : PAGE_COUNT - 1; uiRow = 0; waveMonitorTableSelect = false; }
  if (btn == &btn2) { uiPage = (uiPage + 1) % PAGE_COUNT; uiRow = 0; waveMonitorTableSelect = false; }
  if (btn == &btn3) { if (uiRow > 0) uiRow--; }
  if (btn == &btn4) { uint8_t maxRows = countParamsOnPage(uiPage); if (maxRows > 0 && uiRow < maxRows - 1) uiRow++; }

  uint8_t p = getSelectedParam();
  if (btn == &btn5) {
    stepUiParam(p, -1);
  }

  if (btn == &btn6) {
    stepUiParam(p, 1);
  }
  if (btn == &btn7) {
    if (uiPage == PAGE_PROGRAM) openPresetBrowser();
    else safeLoadProgram((currentProgramNumber + 1) & 127);
  }
  if (btn == &btn8) openUserSaveMode();
}


void pollButtons() {
  btn1.read(); btn2.read(); btn3.read(); btn4.read(); btn5.read(); btn6.read(); btn7.read(); btn8.read();
}


void pollEncoder() {
  int a = digitalRead(ENC_A_PIN);

  if (a != lastEncA && a == LOW) {
    int delta = digitalRead(ENC_B_PIN) == HIGH ? 1 : -1;

    if (userSaveMode) {
      if (userSaveConfirmOverwrite) {
        lastEncA = a;
        return;
      }
      if (userNameEditMode) moveNameCursor(delta > 0 ? 1 : -1);
      else moveUserSaveSlot(delta > 0 ? 1 : -1);
      lastEncA = a;
      return;
    }

    if (presetBrowserActive) {
      browserMoveProgram(delta > 0 ? 1 : -1);
      lastEncA = a;
      return;
    }

    // WAVE MONITOR has its own encoder behavior:
    // - normal mode: encoder scans the wave position
    // - table-select mode: encoder changes the wavetable
    // This avoids the old problem where uiRow=P_WAVETABLE made the encoder
    // always change the table, even when table-select mode was off.
    if (uiPage == PAGE_WAVE_MON) {
      if (waveMonitorTableSelect) {
        stepSelectedWavetable(delta > 0 ? 1 : -1);
      } else {
        // Wave Monitor displays logical waves 0..63 while P_WAVE_POS is 0..127.
        // Use a 2-step increment so every encoder detent changes the visible wave.
        int v = getAParam(P_WAVE_POS) + (delta * 2);
        setParam(P_WAVE_POS, constrain(v, 0, 127));
      }
      lastEncA = a;
      return;
    }

    uint8_t p = getSelectedParam();

    stepUiParam(p, delta > 0 ? 1 : -1);
  }
  lastEncA = a;
}

void pollEncoderButton() {
  static uint32_t downTime = 0;
  static bool wasDown = false;
  static bool longActionDone = false;
  bool down = digitalRead(ENC_SW_PIN) == LOW;

  if (down && !wasDown) {
    downTime = millis();
    longActionDone = false;
  }

  // Immediate long-press action while the button is still held.
  // This feels more direct than waiting until button release.
  if (down && !longActionDone && (millis() - downTime >= 1000)) {
    longActionDone = true;

    if (ccLearnMode) {
      cancelCcLearn();
      wasDown = down;
      return;
    }

    if (userSaveMode) {
      if (userSaveConfirmOverwrite) clearUserSaveOverwriteConfirm();
      else if (userNameEditMode) cancelUserNameEdit();
      else closeUserSaveMode();
      wasDown = down;
      return;
    }

    if (presetBrowserActive) {
      closePresetBrowser();
      wasDown = down;
      return;
    }

    // No CC Learn in WAVE MONITOR; long press simply leaves table-select mode.
    if (uiPage == PAGE_WAVE_MON) {
      waveMonitorTableSelect = false;
      wasDown = down;
      return;
    }

    {
      uint8_t lp = getSelectedParam();
      if (lp < PARAM_COUNT) startCcLearn(lp);
    }
    wasDown = down;
    return;
  }

  if (!down && wasDown) {
    // If a long-press action already fired, release must do nothing.
    if (longActionDone) {
      wasDown = down;
      return;
    }

    if (ccLearnMode) {
      // Short release while in CC Learn: ignore, so no accidental action.
      wasDown = down;
      return;
    }

    if (userSaveMode) {
      if (userSaveConfirmOverwrite) {
        saveCurrentProgramToUserSlot();
      } else if (userNameEditMode) {
        finishUserNameEdit();
      } else {
        startUserNameEdit();
      }
      wasDown = down;
      return;
    }

    if (presetBrowserActive) {
      loadBrowserProgram();
      wasDown = down;
      return;
    }

    // WAVE MONITOR short press toggles table-select mode.
    if (uiPage == PAGE_WAVE_MON) {
      waveMonitorTableSelect = !waveMonitorTableSelect;
      wasDown = down;
      return;
    }

    // Normal short press: next parameter row.
    uint8_t maxRows = countParamsOnPage(uiPage);
    if (maxRows > 0) uiRow = (uiRow + 1) % maxRows;
  }
  wasDown = down;
}

void openPresetBrowser() {
  refreshPresetBrowserNames();
  presetBrowserActive = true;
  browserProgram = currentProgramNumber;
  if (currentProgramNumber < 30) browserCategory = programCategoryForBrowser(currentProgramNumber);
  else browserCategory = BROWSE_USER;
  if (!browserProgramMatches(browserProgram)) {
    for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
      if (browserProgramMatches(i)) {
        browserProgram = i;
        break;
      }
    }
  }
}


void closePresetBrowser() {
  presetBrowserActive = false;
}


static inline void stepSelectedWavetable(int8_t delta) {
  int16_t table = selectedWavetableIndex();
  table += delta;

  if (table < 0) table = 0;
  if (table > WT_LAST_SLOT) table = WT_LAST_SLOT;

  setParam(P_WAVETABLE, (uint8_t)table);
  invalidateWaveMonitorCache();

  int16_t sd = sdIndexForVisibleSlot((uint8_t)table);
  if (sd >= 0) {
    requestSdWavetableCache((uint8_t)sd);
  }
}


void startPresetBrowserNameScan() {
  if (!browserNamesDirty && browserCacheBuildCount > 0) return;

  browserScanActive = true;
  browserScanIndex = 0;
  browserScanLastMs = millis();

  for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
    browserProgramNames[i][0] = 0;
    browserProgramValid[i] = false;

    // Factory slots always exist. Their names are cheap enough to prefill.
    if (i < 30) {
      if (!readProgramNameForBrowser(i, browserProgramNames[i], PROGRAM_NAME_LEN)) {
        snprintf(browserProgramNames[i], PROGRAM_NAME_LEN, "FACTORY %03d", i);
      }
      browserProgramValid[i] = true;
    }
  }
}


void refreshPresetBrowserNames() {
  // Backward-compatible name: now it only starts the async scan.
  startPresetBrowserNameScan();
}


void stepUiParam(uint8_t p, int8_t delta) {
  if (p == UI_PARAM_CC_MODE) {
    setMidiCcModeGlobal(delta > 0 ? 1 : 0, true);
    return;
  }

  if (p == UI_PARAM_LFO_SHAPE) {
    int v = (int)globalLfoShape + delta;
    if (v < 0) v = 0;
    if (v > 5) v = 5;
    setLfoShapeGlobal((uint8_t)v, true);
    return;
  }

  if (p == UI_PARAM_SEQ_TABLE_MODE) {
    setSeqTableModeGlobal(delta > 0 ? 1 : 0, true);
    return;
  }

  if (p == UI_PARAM_SEQ_STEP) {
    int maxStep = getAParam(P_SEQ_STEPS);
    if (maxStep < 1) maxStep = 1;
    if (maxStep > MODSEQ_STEPS) maxStep = MODSEQ_STEPS;

    int s = (int)modSeqEditStep + delta;
    if (s < 0) s = 0;
    if (s >= maxStep) s = maxStep - 1;
    modSeqEditStep = (uint8_t)s;
    return;
  }

  if (p == UI_PARAM_SEQ_VALUE) {
    int v = (int)modSeqValues[modSeqEditStep] + delta;
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    modSeqValues[modSeqEditStep] = (uint8_t)v;
    updateModSeqAudioMirror();
    return;
  }

  if (p == P_CLOCK_SOURCE) {
    setClockSourceGlobal(delta > 0 ? 1 : 0, true);
    return;
  }

  if (p == P_WAVETABLE) {
    stepSelectedWavetable(delta > 0 ? 1 : -1);
  } else if (isSwitchParam(p)) {
    setParam(p, delta > 0 ? 127 : 0);
  } else {
    int v = getAParam(p) + delta;
    setParam(p, constrain(v, 0, 127));
  }
}


void finishUserNameEdit() {
  for (int8_t i = PROGRAM_NAME_LEN - 2; i >= 0; i--) {
    if (currentProgram.name[i] == ' ') currentProgram.name[i] = 0;
    else break;
  }
  currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;
  if (strlen(currentProgram.name) == 0) {
    snprintf(currentProgram.name, PROGRAM_NAME_LEN, "USER %03d", userSaveSlot);
  }
  strncpy(userNameEditBackup, currentProgram.name, PROGRAM_NAME_LEN - 1);
  userNameEditBackup[PROGRAM_NAME_LEN - 1] = 0;
  userNameEditMode = false;
}


void setupButtons() {
  BfButton *buttons[] = {&btn1, &btn2, &btn3, &btn4, &btn5, &btn6, &btn7, &btn8};
  for (auto b : buttons) {
    b->onPress(buttonHandler);
    b->onDoublePress(buttonHandler);
    b->onPressFor(buttonHandler, 1000);
  }
}


void setupEncoder() {
  pinMode(ENC_A_PIN, INPUT_PULLUP);
  pinMode(ENC_B_PIN, INPUT_PULLUP);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  lastEncA = digitalRead(ENC_A_PIN);
}
