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
  // RTAL UI Engine prototype owns the eight main buttons while active.
  // Legacy modal screens (preset browser/save) remain handled below.
  if (rtalUiIsActive() && !userSaveMode && !presetBrowserActive) {
    uint8_t buttonIndex = 0;
    if (btn == &btn1) buttonIndex = 1;
    else if (btn == &btn2) buttonIndex = 2;
    else if (btn == &btn3) buttonIndex = 3;
    else if (btn == &btn4) buttonIndex = 4;
    else if (btn == &btn5) buttonIndex = 5;
    else if (btn == &btn6) buttonIndex = 6;
    else if (btn == &btn7) buttonIndex = 7;
    else if (btn == &btn8) buttonIndex = 8;

    if (pattern == BfButton::SINGLE_PRESS) {
      if (rtalUiHandleButton(buttonIndex, false)) return;
    } else if (pattern == BfButton::LONG_PRESS) {
      if (rtalUiHandleButton(buttonIndex, true)) return;
    }
  }
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

    if (rtalUiIsActive()) {
      // FIX23: three-stage STORE workflow: SLOT -> NAME -> CONFIRM.
      if (userSaveStage == USER_SAVE_SLOT) {
        if (btn == &btn5) { enterUserSaveNameStage(); return; }  // STORE = next
        if (btn == &btn8) { closeUserSaveMode(); return; }       // BACK = cancel
        if (btn == &btn3 || btn == &btn6) { moveUserSaveSlot(-1); return; }
        if (btn == &btn4 || btn == &btn7) { moveUserSaveSlot(1); return; }
        return;
      }

      if (userSaveStage == USER_SAVE_NAME) {
        if (btn == &btn1) { deleteNameChar(); return; }       // SOUND = delete
        if (btn == &btn2) { clearProgramName(); return; }     // EDIT = clear
        if (btn == &btn3) { moveNameCursor(-1); return; }     // MOD = cursor left
        if (btn == &btn4) { moveNameCursor(1); return; }      // FX = cursor right
        if (btn == &btn5) { enterUserSaveConfirmStage(); return; } // STORE = next
        if (btn == &btn6) { stepNameChar(-1); return; }       // LOAD = previous char
        if (btn == &btn7) { stepNameChar(1); return; }        // PLAY = next char
        if (btn == &btn8) { cancelUserNameEdit(); userSaveStage = USER_SAVE_SLOT; return; }
        return;
      }

      if (btn == &btn5) { saveCurrentProgramToUserSlot(); return; } // STORE = confirm
      if (btn == &btn8) { returnToUserSaveSlotStage(); return; }    // BACK = slot page
      return;
    }

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

    // FIX20: physical front-panel mapping for the RTAL preset browser.
    // SOUND/EDIT select the category, MOD/FX select an entry,
    // LOAD or PLAY loads it and BACK leaves the browser unchanged.
    if (btn == &btn1) { browserMoveCategory(-1); return; }
    if (btn == &btn2) { browserMoveCategory(1); return; }
    if (btn == &btn3) { browserMoveProgram(-1); return; }
    if (btn == &btn4) { browserMoveProgram(1); return; }
    if (btn == &btn6 || btn == &btn7) { loadBrowserProgram(); return; }
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
    if (uiPage == PAGE_PROGRAM) {
      openPresetBrowser();
    } else {
      safeLoadProgram((currentProgramNumber + 1) & 127);
    }
  }
  if (btn == &btn8) openUserSaveMode();
}


void pollButtons() {
  // Hold BTN1 + BTN2 for 1.2 seconds to compare legacy v1.1 and RTAL UI Engine.
  // While the chord is held, BfButton does not receive the individual presses.
  static uint32_t uiChordStart = 0;
  static bool uiChordLatched = false;
  const bool chordDown = (digitalRead(BTN_1_PIN) == LOW && digitalRead(BTN_2_PIN) == LOW);
  if (chordDown) {
    if (uiChordStart == 0) uiChordStart = millis();
    if (!uiChordLatched && millis() - uiChordStart >= 1200) {
      uiChordLatched = true;
      rtalUiToggle();
    }
    return;
  }
  uiChordStart = 0;
  uiChordLatched = false;

  btn1.read(); btn2.read(); btn3.read(); btn4.read();
  btn5.read(); btn6.read(); btn7.read(); btn8.read();
}


void pollEncoder() {
  int a = digitalRead(ENC_A_PIN);

  if (a != lastEncA && a == LOW) {
    int delta = digitalRead(ENC_B_PIN) == HIGH ? 1 : -1;

    if (userSaveMode) {
      if (rtalUiIsActive()) {
        if (userSaveStage == USER_SAVE_SLOT) moveUserSaveSlot(delta > 0 ? 1 : -1);
        else if (userSaveStage == USER_SAVE_NAME) stepNameChar(delta > 0 ? 1 : -1);
      } else {
        if (userSaveConfirmOverwrite) {
          lastEncA = a;
          return;
        }
        if (userNameEditMode) moveNameCursor(delta > 0 ? 1 : -1);
        else moveUserSaveSlot(delta > 0 ? 1 : -1);
      }
      lastEncA = a;
      return;
    }

    if (presetBrowserActive) {
      browserMoveProgram(delta > 0 ? 1 : -1);
      lastEncA = a;
      return;
    }

    if (rtalUiHandleEncoder(delta)) {
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

    if (!userSaveMode && !presetBrowserActive && rtalUiHandleEncoderPress(true)) {
      wasDown = down;
      return;
    }

    if (ccLearnMode) {
      cancelCcLearn();
      wasDown = down;
      return;
    }

    if (userSaveMode) {
      if (rtalUiIsActive()) closeUserSaveMode();
      else if (userSaveConfirmOverwrite) clearUserSaveOverwriteConfirm();
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

    if (!userSaveMode && !presetBrowserActive && rtalUiHandleEncoderPress(false)) {
      wasDown = down;
      return;
    }

    if (userSaveMode) {
      if (rtalUiIsActive()) {
        if (userSaveStage == USER_SAVE_SLOT) enterUserSaveNameStage();
        else if (userSaveStage == USER_SAVE_NAME) moveNameCursor(1);
        else saveCurrentProgramToUserSlot();
      } else if (userSaveConfirmOverwrite) {
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
  browserTargetPart = RTAL_BROWSER_TARGET_SINGLE;
  refreshPresetBrowserNames();
  presetBrowserActive = true;
  browserProgram = currentProgramNumber;

  // FIX21: The browser always starts in ALL. The current program remains
  // selected as the initial entry, so opening the browser does not jump
  // away from the sound that is currently loaded.
  browserCategory = BROWSE_ALL;
  if (!browserProgramMatches(browserProgram)) {
    for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
      if (browserProgramMatches(i)) {
        browserProgram = i;
        break;
      }
    }
  }
}

void openPresetBrowserForPart(uint8_t partIndex) {
  if (!multiModeActive() || partIndex >= RTAL_MULTI_PART_COUNT) {
    openPresetBrowser();
    return;
  }
  browserTargetPart = partIndex;
  refreshPresetBrowserNames();
  presetBrowserActive = true;
  browserProgram = multiParts[partIndex].presetNumber;
  browserCategory = BROWSE_ALL;
  if (!browserProgramMatches(browserProgram)) {
    for (uint8_t i = 0; i < NUM_PROGRAMS; ++i) {
      if (browserProgramMatches(i)) { browserProgram = i; break; }
    }
  }
}

void closePresetBrowser() {
  presetBrowserActive = false;
  browserTargetPart = RTAL_BROWSER_TARGET_SINGLE;
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

  if (p == P_LFO_SHAPE) {
    // LFO Shape is a regular ParamId; the wrapper preserves the existing UI path.
    restoreCompareEditIfNeeded();
    int v = (int)getAParam(P_LFO_SHAPE) + delta;
    if (v < 0) v = 0;
    if (v > 5) v = 5;
    setLfoShapeGlobal((uint8_t)v, true);
    return;
  }

  if (p == UI_PARAM_SEQ_TABLE_MODE) {
    const uint8_t v = delta > 0 ? 1 : 0;
    setSeqTableModeGlobal(v, true);
    // FIX4: normal F/U performance is rendered by Multi Part 1. Keep the
    // program editor copy and the executing Part-1 copy identical.
    if (multiModeActive() && currentPerformanceStateKind == LAST_STATE_PROGRAM)
      multiSeqSetTableMode(0, v);
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
    // FIX4: mirror the edited step into the actual Part-1 runtime/program
    // used for a normal F/U preset, so the change is immediately audible and
    // STORE/LOAD cannot diverge from SEQ SHOW.
    if (multiModeActive() && currentPerformanceStateKind == LAST_STATE_PROGRAM)
      multiSeqSetValue(0, modSeqEditStep, (uint8_t)v);
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

// ================================================================
// RTAL Remote A002 - queued front-panel injection
// Called only by ControlTask on Core 0. Network code never calls UI/synth code.
// ================================================================
static BfButton *remoteButtonByIndex(uint8_t n) {
  switch (n) { case 1:return &btn1; case 2:return &btn2; case 3:return &btn3; case 4:return &btn4;
               case 5:return &btn5; case 6:return &btn6; case 7:return &btn7; case 8:return &btn8; default:return nullptr; }
}

static void remoteEncoderDelta(int delta) {
  delta = delta >= 0 ? 1 : -1;
  if (userSaveMode) {
    if (rtalUiIsActive()) {
      if (userSaveStage == USER_SAVE_SLOT) moveUserSaveSlot(delta);
      else if (userSaveStage == USER_SAVE_NAME) stepNameChar(delta);
    } else {
      if (userSaveConfirmOverwrite) return;
      if (userNameEditMode) moveNameCursor(delta); else moveUserSaveSlot(delta);
    }
    return;
  }
  if (presetBrowserActive) { browserMoveProgram(delta); return; }
  if (rtalUiHandleEncoder(delta)) return;
  if (uiPage == PAGE_WAVE_MON) {
    if (waveMonitorTableSelect) stepSelectedWavetable(delta);
    else setParam(P_WAVE_POS, constrain((int)getAParam(P_WAVE_POS) + delta * 2, 0, 127));
    return;
  }
  stepUiParam(getSelectedParam(), delta);
}

static void remoteEncoderPress(bool longPress) {
  if (!userSaveMode && !presetBrowserActive && rtalUiHandleEncoderPress(longPress)) return;
  if (longPress) {
    if (ccLearnMode) { cancelCcLearn(); return; }
    if (userSaveMode) { closeUserSaveMode(); return; }
    if (presetBrowserActive) { closePresetBrowser(); return; }
    if (uiPage == PAGE_WAVE_MON) { waveMonitorTableSelect = false; return; }
    uint8_t p = getSelectedParam(); if (p < PARAM_COUNT) startCcLearn(p); return;
  }
  if (ccLearnMode) return;
  if (userSaveMode) {
    if (rtalUiIsActive()) {
      if (userSaveStage == USER_SAVE_SLOT) enterUserSaveNameStage();
      else if (userSaveStage == USER_SAVE_NAME) moveNameCursor(1);
      else saveCurrentProgramToUserSlot();
    } else if (userSaveConfirmOverwrite) saveCurrentProgramToUserSlot();
    else if (userNameEditMode) finishUserNameEdit(); else startUserNameEdit();
    return;
  }
  if (presetBrowserActive) { loadBrowserProgram(); return; }
  if (uiPage == PAGE_WAVE_MON) { waveMonitorTableSelect = !waveMonitorTableSelect; return; }
  uint8_t maxRows = countParamsOnPage(uiPage); if (maxRows > 0) uiRow = (uiRow + 1) % maxRows;
}

static bool remoteFxParamAllowed(uint8_t p) {
  using namespace RTALFxProtocol;
  if (p >= P_DELAY_ENABLE && p <= P_DELAY_RIGHT_DIV) return true;
  if (p >= P_MOD_ENABLE && p <= P_MOD_SMOOTHING_MS) return true;
  if (p == P_MOD_EFFECT_SELECT) return true;
  if (p >= P_CHORUS_ENABLE && p <= P_CHORUS_TONE_HZ) return true;
  if (p >= P_FLANGER_ENABLE && p <= P_FLANGER_SATURATION) return true;
  if (p >= P_PHASER_ENABLE && p <= P_PHASER_CENTER_HZ) return true;
  if (p >= P_WIDTH_ENABLE && p <= P_WIDTH_AMOUNT) return true;
  if (p >= P_REVERB_ENABLE && p <= P_REVERB_PREDELAY_MS) return true;
  return false;
}

void pollRemoteControl() {
#if RTAL_REMOTE_USB_ENABLED
  RTALRemote::Command c;
  uint8_t budget = 6; // bound work per ControlTask iteration
  while (budget-- && RTALRemote::popCommand(c)) {
    if (c.type == RTALRemote::CMD_BUTTON_SHORT || c.type == RTALRemote::CMD_BUTTON_LONG) {
      BfButton *b = remoteButtonByIndex((uint8_t)c.value);
      if (b) buttonHandler(b, c.type == RTALRemote::CMD_BUTTON_LONG ? BfButton::LONG_PRESS : BfButton::SINGLE_PRESS);
    } else if (c.type == RTALRemote::CMD_ENCODER_DELTA) remoteEncoderDelta(c.value);
    else if (c.type == RTALRemote::CMD_ENCODER_SHORT) remoteEncoderPress(false);
    else if (c.type == RTALRemote::CMD_ENCODER_LONG) remoteEncoderPress(true);
    else if (c.type == RTALRemote::CMD_PERFORMANCE_MODE) {
      const PerformanceMode target = c.value ? PerformanceMode::MULTI : PerformanceMode::SINGLE;
      const bool ok = switchPerformanceMode(target);
      rtalUiShowMessage(ok ? (target == PerformanceMode::MULTI ? "MODE MULTI" : "MODE SINGLE") : "MULTI EMPTY", 1000);
    }
    else if (c.type == RTALRemote::CMD_FX_REQUEST_STATE) RTALFxLink::requestState();
    else if (c.type == RTALRemote::CMD_FX_SET) {
      if (remoteFxParamAllowed(c.paramId)) {
        using namespace RTALFxProtocol;
        if (c.paramId == P_MOD_ENABLE) {
          // A014A_FIX1: MOD engine is derived from Effect Select, never independently switched.
          if (c.value == 0) RTALFxLink::selectModEffect(0);
        }
        else if (c.paramId == P_CHORUS_ENABLE) RTALFxLink::setModEffectEnabled(1, c.value != 0);
        else if (c.paramId == P_FLANGER_ENABLE) RTALFxLink::setModEffectEnabled(2, c.value != 0);
        else if (c.paramId == P_PHASER_ENABLE) RTALFxLink::setModEffectEnabled(3, c.value != 0);
        else if (c.paramId == P_MOD_EFFECT_SELECT) RTALFxLink::selectModEffect((uint8_t)constrain((int)c.value, 0, 3));
        else RTALFxLink::setInt(c.paramId, c.value);
      }
    }
    else if (c.type == RTALRemote::CMD_SEQ_SET) {
      const uint8_t step = c.paramId;
      const uint8_t value = c.paramValue;
      if (step < MODSEQ_STEPS) {
        const uint8_t runtimePart = multiPerformanceContextActive() ? rtalUiSelectedMultiPart() : 0;
        if (multiPerformanceContextActive()) {
          multiSeqSetValue(runtimePart, step, value);
        } else {
          modSeqValues[step] = value;
          multiSeqSetValue(0, step, value);
          updateModSeqAudioMirror();
        }
      }
    }
    else if (c.type == RTALRemote::CMD_SEQ_TABLE_MODE) {
      const uint8_t mode = c.paramValue ? 1 : 0;
      const uint8_t runtimePart = multiPerformanceContextActive() ? rtalUiSelectedMultiPart() : 0;
      if (multiPerformanceContextActive()) {
        multiSeqSetTableMode(runtimePart, mode);
      } else {
        setSeqTableModeGlobal(mode, true);
        multiSeqSetTableMode(0, mode);
      }
    }
    else if (c.type == RTALRemote::CMD_SYSTEM_SET) {
      const uint8_t item = c.paramId;
      const uint8_t value = c.paramValue;
      if (item == 0 && value >= 1 && value <= 16) {
        // A011_FIX2: Normal PROGRAM performance is rendered by Multi Part 1.
        // Keep the global MIDI channel and Part 1 routing synchronized.
        // Kill currently sounding Part-1 notes before changing the channel so
        // their later Note-Off cannot be lost on the old channel.
        if (!multiPerformanceContextActive() && value != midiChannel) {
          allNotesOffForPart(0, true);
        }
        midiChannel = value;
        if (!multiPerformanceContextActive()) {
          multiParts[0].midiChannel = midiChannel;
        }
        prefs.putUChar("midiCh", midiChannel);
        saveSystemConfigToSD();
      } else if (item == 1 && value <= 1) {
        setMidiCcModeGlobal(value, true);
      } else if (item == 2 && value <= 2) {
        midiCcBank = value;
        prefs.putUChar("ccBank", midiCcBank);
        saveSystemConfigToSD();
      }
    }
    else if (c.type == RTALRemote::CMD_MULTI_SELECT) {
      if (c.paramId < RTAL_MULTI_PART_COUNT) rtalUiSetSelectedMultiPart(c.paramId);
    }
    else if (c.type == RTALRemote::CMD_MULTI_NAME_QUERY) {
      const uint8_t slot = c.paramId;
      if (slot < RTAL_MULTI_SLOT_COUNT) {
        RTALRemote::MultiNameState ns = {};
        ns.slot = slot;
        if (sdCardOk) ns.flags |= 0x02;
        char storedName[PROGRAM_NAME_LEN] = {0};
        if (readMultiSetupNameFromSD(slot, storedName, sizeof(storedName))) {
          ns.flags |= 0x01;
          strncpy(ns.name, storedName, sizeof(ns.name) - 1);
        }
        RTALRemote::publishMultiNameState(ns);
      }
    }
    else if (c.type == RTALRemote::CMD_MULTI_LOAD) {
      const uint8_t slot = c.paramId;
      if (slot < RTAL_MULTI_SLOT_COUNT) {
        if (safeLoadMultiSetup(slot)) rtalUiSetSelectedMultiPart(0);
      }
    }
    else if (c.type == RTALRemote::CMD_MULTI_SAVE) {
      const uint8_t slot = c.paramId;
      if (slot < RTAL_MULTI_SLOT_COUNT && multiPerformanceContextActive()) {
        char cleanName[PROGRAM_NAME_LEN] = {0};
        strncpy(cleanName, c.text, PROGRAM_NAME_LEN - 1);
        cleanName[PROGRAM_NAME_LEN - 1] = 0;
        for (uint8_t i = 0; cleanName[i]; ++i) {
          if (cleanName[i] == ',' || cleanName[i] == '\r' || cleanName[i] == '\n') cleanName[i] = ' ';
        }
        if (!cleanName[0]) snprintf(cleanName, PROGRAM_NAME_LEN, "MULTI %03u", slot);
        strncpy(currentMultiName, cleanName, PROGRAM_NAME_LEN - 1);
        currentMultiName[PROGRAM_NAME_LEN - 1] = 0;
        saveMultiSetupToSD(slot);
      }
    }
    else if (c.type == RTALRemote::CMD_MULTI_PRESET) {
      if (!multiPerformanceContextActive()) continue;
      const uint8_t part = c.paramId;
      const uint8_t slot = c.paramValue & 0x7F;
      if (part < RTAL_MULTI_PART_COUNT) {
        Program tmp;
        if (readProgramForMulti(slot, tmp)) {
          applyPresetToMultiPart(part, slot, tmp, "REMOTE PRESET");
          rtalUiSetSelectedMultiPart(part);
        }
      }
    }
    else if (c.type == RTALRemote::CMD_MULTI_SET) {
      if (!multiPerformanceContextActive()) continue;
      const uint8_t partIndex = c.paramId;
      const uint8_t item = c.paramValue;
      const int32_t value = c.value;
      if (partIndex < RTAL_MULTI_PART_COUNT && item <= 10) {
        MultiPart &part = multiParts[partIndex];
        if (item == 0 && value >= 1 && value <= 16) {
          if (part.midiChannel != (uint8_t)value) killMultiPartForReason(partIndex, "REMOTE CHANNEL");
          part.midiChannel = (uint8_t)value;
        } else if (item == 1 && value >= 0 && value <= 127) {
          part.volume = (uint8_t)value;
        } else if (item == 2 && value >= -24 && value <= 24) {
          part.transpose = (int8_t)value;
        } else if (item == 3 && value >= 0 && value <= NUM_VOICES) {
          uint8_t other = 0;
          for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) if (p != partIndex) other += multiParts[p].voiceReserve;
          if ((uint8_t)value + other <= NUM_VOICES) part.voiceReserve = (uint8_t)value;
        } else if (item == 4 && (value == 0 || value == 1)) {
          const bool next = value != 0;
          const bool wasEnabled = part.enabled;
          part.enabled = next;  // publish the gate state before MidiTask can step the ARP
          if (wasEnabled && !next) silenceMultiPartForPerformance(partIndex, "REMOTE DISABLE");
        } else if (item == 5 && (value == 0 || value == 1)) {
          const bool next = value != 0;
          const bool wasMute = part.mute;
          part.mute = next;     // block ARP stepping before silencing current audio
          if (!wasMute && next) silenceMultiPartForPerformance(partIndex, "REMOTE MUTE");
        } else if (item == 6 && value >= 10 && value <= 100) {
          part.arpGatePct = (uint8_t)value;
        } else if (item == 7 && value >= 0 && value <= 127) {
          uint8_t next = (uint8_t)value;
          if (next > part.keyHigh) next = part.keyHigh;
          if (next != part.keyLow) killMultiPartForReason(partIndex, "REMOTE KEY LOW");
          part.keyLow = next;
        } else if (item == 8 && value >= 0 && value <= 127) {
          uint8_t next = (uint8_t)value;
          if (next < part.keyLow) next = part.keyLow;
          if (next != part.keyHigh) killMultiPartForReason(partIndex, "REMOTE KEY HIGH");
          part.keyHigh = next;
        } else if (item == 9 && value >= 0 && value <= 127) {
          part.rootNote = (uint8_t)value;
          multiVoiceRuntimeGeneration[partIndex]++;
          if (multiVoiceRuntimeGeneration[partIndex] == 0) multiVoiceRuntimeGeneration[partIndex] = 1;
        } else if (item == 10 && value >= -64 && value <= 63) {
          part.pan = (int8_t)value;
          for (uint8_t vi = 0; vi < NUM_VOICES; ++vi) {
            if (!voices[vi].active || voices[vi].partIndex != partIndex) continue;
            const uint8_t spread = multiParam(partIndex, P_PAN_SPREAD);
            int32_t pv = ((vi % 2) ? (int32_t)spread : -(int32_t)spread) << 8;
            pv += ((int32_t)part.pan << 9);
            voices[vi].pan = (int16_t)clip32(pv, -32768, 32767);
            refreshVoicePanRuntime(voices[vi]);
          }
        }
        rtalUiSetSelectedMultiPart(partIndex);
      }
    }
    else if (c.type == RTALRemote::CMD_PRESET_REFRESH) {
      browserNamesDirty = true;
      startPresetBrowserNameScan();
    }
    else if (c.type == RTALRemote::CMD_PRESET_LOAD) {
      const uint8_t slot = c.paramId & 0x7F;
      safeLoadProgram(slot);
      // Refresh this cache entry without turning a missing U-slot into an
      // apparently occupied preset merely because loadProgram() created INIT.
      if (slot < 30) {
        strncpy(browserProgramNames[slot], currentProgram.name, PROGRAM_NAME_LEN - 1);
        browserProgramNames[slot][PROGRAM_NAME_LEN - 1] = 0;
        browserProgramValid[slot] = true;
      } else {
        char cachedName[PROGRAM_NAME_LEN] = {};
        if (readProgramNameForBrowser(slot, cachedName, sizeof(cachedName))) {
          strncpy(browserProgramNames[slot], cachedName, PROGRAM_NAME_LEN - 1);
          browserProgramNames[slot][PROGRAM_NAME_LEN - 1] = 0;
          browserProgramValid[slot] = true;
        } else {
          browserProgramNames[slot][0] = 0;
          browserProgramValid[slot] = false;
        }
      }
    }
    else if (c.type == RTALRemote::CMD_PRESET_SAVE) {
      const uint8_t slot = c.paramId & 0x7F;
      if (slot >= 30 && slot < NUM_PROGRAMS && sdCardOk) {
        restoreCompareEditIfNeeded();
        snapshotAudioParamsToCurrentProgram();
        strncpy(currentProgram.name, c.text, PROGRAM_NAME_LEN - 1);
        currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;
        normalizeProgramName();
        saveProgram(slot);
        currentProgramNumber = slot;
        rememberLastProgram(slot);
        strncpy(browserProgramNames[slot], currentProgram.name, PROGRAM_NAME_LEN - 1);
        browserProgramNames[slot][PROGRAM_NAME_LEN - 1] = 0;
        browserProgramValid[slot] = true;
        showProgramPreview();
      }
    }
    else if (c.type == RTALRemote::CMD_PARAM_SET) {
      const uint8_t paramId = c.paramId;
      const uint8_t paramValue = c.paramValue;

      // SAFE_A007: explicit whitelist of continuous + discrete editor parameters.
      // The USB task never calls setParam() directly.
      if ((paramId == P_WAVE_POS || paramId == P_WAVE_MOD || paramId == P_OSC_MIX || paramId == P_OSC_DETUNE || paramId == P_OSC_B_OFFSET || paramId == P_CUTOFF || paramId == P_RESONANCE || paramId == P_FILTER_ENV || paramId == P_ATTACK || paramId == P_DECAY || paramId == P_SUSTAIN || paramId == P_RELEASE || paramId == P_F_ATTACK || paramId == P_F_DECAY || paramId == P_F_SUSTAIN || paramId == P_F_RELEASE || paramId == P_WAVE_ENV || paramId == P_WAVE_ENV_ATTACK || paramId == P_WAVE_ENV_DECAY || paramId == P_WAVE_ENV_SUSTAIN || paramId == P_WAVE_ENV_RELEASE || paramId == P_LFO_RATE || paramId == P_LFO_AMOUNT || paramId == P_WAVE_LFO_RATE || paramId == P_WAVE_LFO_AMOUNT || paramId == P_AFTERTOUCH_WAVE || paramId == P_AFTERTOUCH_FILTER || paramId == P_VEL_AMP || paramId == P_VEL_FILTER || paramId == P_KEYTRACK || paramId == P_PAN_SPREAD || paramId == P_DRIVE || paramId == P_BITCRUSH || paramId == P_VOLUME || paramId == P_GLIDE || paramId == P_BEND_RANGE || paramId == P_UNISON_DETUNE || paramId == P_SUB_LEVEL || paramId == P_NOISE_LEVEL || paramId == P_CHORUS || paramId == P_ARP_RATE || paramId == P_ARP_OCTAVES || paramId == P_SEQ_RATE || paramId == P_SEQ_STEPS || paramId == P_SEQ_DEPTH || paramId == P_MORPH_AMOUNT) || paramId == P_WAVETABLE || paramId == P_LFO_TARGET || paramId == P_LFO_SHAPE || paramId == P_PLAY_MODE || paramId == P_ARP_MODE || paramId == P_ARP_RATE || paramId == P_ARP_OCTAVES || paramId == P_ARP_HOLD || paramId == P_CLOCK_SOURCE || paramId == P_TAP_TEMPO || paramId == P_SEQ_MODE || paramId == P_SEQ_RATE || paramId == P_SEQ_TARGET || paramId == P_RANDOMIZE) {
        // WB0064 MULTI ROUTE FIX1: all part-owned synth controls belong to
        // the currently selected Multi Part, not only ARP/SEQ. The audio engine
        // renders Multi from multiParts[p].program, so writing global audioParams
        // here made OSC/FILTER/ENV/LFO edits inaudible in Multi.
        // Clock/tempo, Morph and Randomize remain global by current architecture.
        const bool multiPartOwned = multiPerformanceContextActive() &&
          paramId != P_CLOCK_SOURCE && paramId != P_TAP_TEMPO &&
          paramId != P_MORPH_AMOUNT && paramId != P_RANDOMIZE;
        if (multiPartOwned) setMultiPartParam(rtalUiSelectedMultiPart(), paramId, paramValue);
        else setParam(paramId, paramValue);
      }
    }
  }
#endif
}
