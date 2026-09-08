#include "../include/RTALStereoDelay.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

portMUX_TYPE RTALStereoDelay::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALStereoDelayParameters RTALStereoDelay::target_ = {};
RTALStereoDelayParameters RTALStereoDelay::blockTarget_ = {};
RTALStereoDelayParameters RTALStereoDelay::current_ = {};
RTALStereoDelayStatistics RTALStereoDelay::statistics_ = {};
RTALStereoDelayForensics RTALStereoDelay::forensics_ = {};
RTALStereoDelayForensics RTALStereoDelay::forensicWindow_ = {};
uint32_t RTALStereoDelay::forensicFrameIndex_ = 0;

float* RTALStereoDelay::bufferLeft_ = nullptr;
float* RTALStereoDelay::bufferRight_ = nullptr;
uint32_t RTALStereoDelay::bufferSamples_ = 0;
uint32_t RTALStereoDelay::writeIndex_ = 0;
uint32_t RTALStereoDelay::sampleRate_ = 0;

float RTALStereoDelay::feedbackFilterStateLeft_ = 0.0f;
float RTALStereoDelay::feedbackFilterStateRight_ = 0.0f;
float RTALStereoDelay::feedbackLowpassCoefficient_ = 1.0f;
float RTALStereoDelay::feedbackHighpassInputLeft_ = 0.0f;
float RTALStereoDelay::feedbackHighpassInputRight_ = 0.0f;
float RTALStereoDelay::feedbackHighpassStateLeft_ = 0.0f;
float RTALStereoDelay::feedbackHighpassStateRight_ = 0.0f;
float RTALStereoDelay::feedbackHighpassCoefficient_ = 1.0f;
uint32_t RTALStereoDelay::coefficientUpdateCountdown_ = 0;
float RTALStereoDelay::coefficientLowpassHz_ = 0.0f;
float RTALStereoDelay::coefficientHighpassHz_ = 0.0f;
float RTALStereoDelay::duckEnvelope_ = 0.0f;
float RTALStereoDelay::duckGain_ = 1.0f;
float RTALStereoDelay::duckThresholdLinear_ = 0.0630957f;
float RTALStereoDelay::duckReleaseCoefficient_ = 0.999f;
float RTALStereoDelay::freezeMix_ = 0.0f;
bool RTALStereoDelay::syncTransitionActive_ = false;
float RTALStereoDelay::syncOldLeftMs_ = 0.0f;
float RTALStereoDelay::syncOldRightMs_ = 0.0f;
float RTALStereoDelay::syncNewLeftMs_ = 0.0f;
float RTALStereoDelay::syncNewRightMs_ = 0.0f;
float RTALStereoDelay::syncTransitionMix_ = 1.0f;
float RTALStereoDelay::syncTransitionStep_ = 1.0f;

float RTALStereoDelay::clamp(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

float RTALStereoDelay::lowpassCoefficient(float cutoffHz)
{
    const float cutoff = clamp(
        cutoffHz,
        RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MIN,
        RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MAX);

    return 1.0f - expf(
        -2.0f * PI * cutoff / static_cast<float>(sampleRate_));
}

float RTALStereoDelay::highpassCoefficient(float cutoffHz)
{
    const float cutoff = clamp(
        cutoffHz,
        RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MIN,
        RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MAX);

    return static_cast<float>(sampleRate_) /
        (static_cast<float>(sampleRate_) + 2.0f * PI * cutoff);
}

float RTALStereoDelay::dbToLinear(float db)
{
    return powf(10.0f, db / 20.0f);
}

float RTALStereoDelay::saturateFeedback(float value, float amount)
{
    if (amount <= 0.0001f)
        return value;

    const float threshold =
        0.98f -
        amount *
        (0.98f - RTAL_DELAY_FEEDBACK_SATURATION_THRESHOLD_MIN);

    const float magnitude = fabsf(value);

    // Clean region: no coloration below the knee.
    if (magnitude <= threshold)
        return value;

    const float headroom = 1.0f - threshold;
    const float over = magnitude - threshold;

    // Soft knee, unity slope at threshold, asymptotic limit at full scale.
    const float compressed =
        threshold +
        over /
        (1.0f + over / headroom);

    return value < 0.0f ? -compressed : compressed;
}

RTALStatus RTALStereoDelay::begin(uint32_t sampleRate)
{
    sampleRate_ = sampleRate;
    bufferSamples_ =
        static_cast<uint32_t>(
            ceilf(RTAL_DELAY_MAX_MS * sampleRate_ / 1000.0f)) + 2;

    const size_t bytes =
        static_cast<size_t>(bufferSamples_) * sizeof(float);

    bufferLeft_ = static_cast<float*>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    bufferRight_ = static_cast<float*>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!bufferLeft_ || !bufferRight_)
    {
        if (bufferLeft_) heap_caps_free(bufferLeft_);
        if (bufferRight_) heap_caps_free(bufferRight_);
        bufferLeft_ = nullptr;
        bufferRight_ = nullptr;
        return RTALStatus::FATAL;
    }

    memset(bufferLeft_, 0, bytes);
    memset(bufferRight_, 0, bytes);

    target_.enabled = RTAL_STEREO_DELAY_ENABLED;
    target_.freeze = RTAL_DELAY_FREEZE_DEFAULT;
    target_.crossfeed = RTAL_DELAY_CROSSFEED_DEFAULT;
    target_.pingPong = target_.crossfeed >= 0.999f;
    target_.duckAmount = RTAL_DELAY_DUCK_AMOUNT_DEFAULT;
    target_.duckThresholdDb = RTAL_DELAY_DUCK_THRESHOLD_DB_DEFAULT;
    target_.duckReleaseMs = RTAL_DELAY_DUCK_RELEASE_MS_DEFAULT;
    target_.timeLeftMs = RTAL_DELAY_TIME_LEFT_MS_DEFAULT;
    target_.timeRightMs = RTAL_DELAY_TIME_RIGHT_MS_DEFAULT;
    target_.feedback = RTAL_DELAY_FEEDBACK_DEFAULT_BUILD0015;
    target_.level = RTAL_DELAY_LEVEL_DEFAULT;
    target_.feedbackLowpassHz =
        RTAL_DELAY_FEEDBACK_LOWPASS_HZ_DEFAULT;
    target_.feedbackHighpassHz =
        RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_DEFAULT;
    target_.feedbackSaturation =
        RTAL_DELAY_FEEDBACK_SATURATION_DEFAULT;

    blockTarget_ = target_;
    current_ = target_;

    statistics_ = {};
    statistics_.allocatedBytes = bytes * 2;
    statistics_.usingPsram = true;

    writeIndex_ = 0;
    feedbackFilterStateLeft_ = 0.0f;
    feedbackFilterStateRight_ = 0.0f;
    feedbackLowpassCoefficient_ =
        lowpassCoefficient(current_.feedbackLowpassHz);
    coefficientLowpassHz_ = current_.feedbackLowpassHz;
    feedbackHighpassInputLeft_ = 0.0f;
    feedbackHighpassInputRight_ = 0.0f;
    feedbackHighpassStateLeft_ = 0.0f;
    feedbackHighpassStateRight_ = 0.0f;
    feedbackHighpassCoefficient_ =
        highpassCoefficient(current_.feedbackHighpassHz);
    coefficientHighpassHz_ = current_.feedbackHighpassHz;
    coefficientUpdateCountdown_ = 0;
    duckEnvelope_ = 0.0f;
    duckGain_ = 1.0f;
    freezeMix_ = target_.freeze ? 1.0f : 0.0f;
    syncTransitionActive_ = false;
    syncTransitionMix_ = 1.0f;
    syncOldLeftMs_ = current_.timeLeftMs;
    syncOldRightMs_ = current_.timeRightMs;
    syncNewLeftMs_ = current_.timeLeftMs;
    syncNewRightMs_ = current_.timeRightMs;
    duckThresholdLinear_ =
        dbToLinear(current_.duckThresholdDb);
    const float releaseSamples =
        (current_.duckReleaseMs * 0.001f) *
        static_cast<float>(sampleRate_);
    duckReleaseCoefficient_ =
        releaseSamples /
        (releaseSamples + 1.0f);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "StereoDelay .... PASS L=%.1fms R=%.1fms feedback=%.3f SAT=%.2f HPF=%.0fHz LPF=%.0fHz crossfeed=%.3f level=%.3f bytes=%lu mem=PSRAM",
        target_.timeLeftMs,
        target_.timeRightMs,
        target_.feedback,
        target_.feedbackSaturation,
        target_.feedbackHighpassHz,
        target_.feedbackLowpassHz,
        target_.crossfeed,
        target_.level,
        static_cast<unsigned long>(statistics_.allocatedBytes));

    return RTALStatus::OK;
}

void RTALStereoDelay::reset()
{
    if (!bufferLeft_ || !bufferRight_) return;

    const size_t bytes =
        static_cast<size_t>(bufferSamples_) * sizeof(float);

    memset(bufferLeft_, 0, bytes);
    memset(bufferRight_, 0, bytes);

    writeIndex_ = 0;
    feedbackFilterStateLeft_ = 0.0f;
    feedbackFilterStateRight_ = 0.0f;
    feedbackHighpassInputLeft_ = 0.0f;
    feedbackHighpassInputRight_ = 0.0f;
    feedbackHighpassStateLeft_ = 0.0f;
    feedbackHighpassStateRight_ = 0.0f;
    freezeMix_ = target_.freeze ? 1.0f : 0.0f;
    syncTransitionActive_ = false;
    syncTransitionMix_ = 1.0f;
    syncOldLeftMs_ = current_.timeLeftMs;
    syncOldRightMs_ = current_.timeRightMs;
    syncNewLeftMs_ = current_.timeLeftMs;
    syncNewRightMs_ = current_.timeRightMs;
}

void RTALStereoDelay::setEnabled(bool value)
{
    portENTER_CRITICAL(&mux_);
    target_.enabled = value;
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setFreeze(bool value)
{
    portENTER_CRITICAL(&mux_);
    target_.freeze = value;
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setPingPong(bool value)
{
    portENTER_CRITICAL(&mux_);
    target_.crossfeed = value ? 1.0f : 0.0f;
    target_.pingPong = value;
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setCrossfeed(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.crossfeed = clamp(value, RTAL_DELAY_CROSSFEED_MIN, RTAL_DELAY_CROSSFEED_MAX);
    target_.pingPong = target_.crossfeed >= 0.999f;
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setDuckAmount(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.duckAmount = clamp(value, RTAL_DELAY_DUCK_AMOUNT_MIN, RTAL_DELAY_DUCK_AMOUNT_MAX);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setDuckThresholdDb(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.duckThresholdDb = clamp(value, RTAL_DELAY_DUCK_THRESHOLD_DB_MIN, RTAL_DELAY_DUCK_THRESHOLD_DB_MAX);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setDuckReleaseMs(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.duckReleaseMs = clamp(value, RTAL_DELAY_DUCK_RELEASE_MS_MIN, RTAL_DELAY_DUCK_RELEASE_MS_MAX);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setTimeLeftMs(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.timeLeftMs = clamp(value, 1.0f, RTAL_DELAY_MAX_MS);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setTimeRightMs(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.timeRightMs = clamp(value, 1.0f, RTAL_DELAY_MAX_MS);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::transitionSyncTimes(float leftMs, float rightMs, float crossfadeMs)
{
    const float newLeft = clamp(leftMs, 1.0f, RTAL_DELAY_MAX_MS);
    const float newRight = clamp(rightMs, 1.0f, RTAL_DELAY_MAX_MS);
    const float durationMs = clamp(crossfadeMs, 1.0f, 200.0f);

    portENTER_CRITICAL(&mux_);
    // If a transition is already in progress, start from the currently dominant
    // destination to avoid chasing a moving read head. This path is only used
    // for accepted clock changes, not for normal FREE/manual edits.
    syncOldLeftMs_ = syncTransitionActive_ && syncTransitionMix_ >= 0.5f
        ? syncNewLeftMs_ : current_.timeLeftMs;
    syncOldRightMs_ = syncTransitionActive_ && syncTransitionMix_ >= 0.5f
        ? syncNewRightMs_ : current_.timeRightMs;
    syncNewLeftMs_ = newLeft;
    syncNewRightMs_ = newRight;
    target_.timeLeftMs = newLeft;
    target_.timeRightMs = newRight;
    syncTransitionMix_ = 0.0f;
    const float samples = fmaxf(1.0f, durationMs * 0.001f * static_cast<float>(sampleRate_));
    syncTransitionStep_ = 1.0f / samples;
    syncTransitionActive_ = true;
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setFeedback(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.feedback = clamp(value, 0.0f, 0.92f);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setLevel(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.level = clamp(value, 0.0f, 1.0f);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setFeedbackLowpassHz(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.feedbackLowpassHz = clamp(
        value,
        RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MIN,
        RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MAX);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setFeedbackHighpassHz(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.feedbackHighpassHz = clamp(
        value,
        RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MIN,
        RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MAX);
    portEXIT_CRITICAL(&mux_);
}

void RTALStereoDelay::setFeedbackSaturation(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.feedbackSaturation = clamp(
        value,
        RTAL_DELAY_FEEDBACK_SATURATION_MIN,
        RTAL_DELAY_FEEDBACK_SATURATION_MAX);
    portEXIT_CRITICAL(&mux_);
}

RTALStereoDelayParameters RTALStereoDelay::parameters()
{
    portENTER_CRITICAL(&mux_);
    const RTALStereoDelayParameters copy = target_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

void RTALStereoDelay::beginBlock(size_t frames)
{
    portENTER_CRITICAL(&mux_);
    blockTarget_ = target_;
    ++statistics_.processedBlocks;
    portEXIT_CRITICAL(&mux_);

    if (RTAL_DELAY_FORENSICS_ENABLED)
    {
        forensics_ = {};
        forensics_.totalFrames = static_cast<uint32_t>(frames);
        forensicFrameIndex_ = 0;
    }

    // Derived ducking coefficients are updated once per block.
    duckThresholdLinear_ =
        dbToLinear(blockTarget_.duckThresholdDb);

    const float releaseSamples =
        (blockTarget_.duckReleaseMs * 0.001f) *
        static_cast<float>(sampleRate_);

    duckReleaseCoefficient_ =
        releaseSamples /
        (releaseSamples + 1.0f);
}

void RTALStereoDelay::updateSmoothedParameters()
{
    const float a = RTAL_DELAY_PARAMETER_SMOOTHING_BUILD0015;

    current_.enabled = blockTarget_.enabled;
    current_.freeze = blockTarget_.freeze;
    current_.crossfeed +=
        (blockTarget_.crossfeed - current_.crossfeed) * a;
    current_.duckAmount +=
        (blockTarget_.duckAmount - current_.duckAmount) * a;
    current_.duckThresholdDb +=
        (blockTarget_.duckThresholdDb - current_.duckThresholdDb) * a;
    current_.duckReleaseMs +=
        (blockTarget_.duckReleaseMs - current_.duckReleaseMs) * a;
    current_.pingPong = current_.crossfeed >= 0.999f;
    if (!syncTransitionActive_)
    {
        current_.timeLeftMs +=
            (blockTarget_.timeLeftMs - current_.timeLeftMs) * a;
        current_.timeRightMs +=
            (blockTarget_.timeRightMs - current_.timeRightMs) * a;
    }
    current_.feedback +=
        (blockTarget_.feedback - current_.feedback) * a;
    current_.level +=
        (blockTarget_.level - current_.level) * a;
    current_.feedbackLowpassHz +=
        (blockTarget_.feedbackLowpassHz -
         current_.feedbackLowpassHz) * a;
    current_.feedbackHighpassHz +=
        (blockTarget_.feedbackHighpassHz -
         current_.feedbackHighpassHz) * a;
    current_.feedbackSaturation +=
        (blockTarget_.feedbackSaturation -
         current_.feedbackSaturation) * a;

}

void RTALStereoDelay::updateFilterCoefficientsIfDue()
{
    if (coefficientUpdateCountdown_ > 0)
    {
        --coefficientUpdateCountdown_;
        return;
    }

    coefficientUpdateCountdown_ = RTAL_DELAY_COEFFICIENT_UPDATE_INTERVAL - 1;

    // Recompute only while a cutoff is actually moving. At steady state this
    // removes the expensive expf() from the sample loop completely.
    if (current_.feedbackLowpassHz != coefficientLowpassHz_)
    {
        feedbackLowpassCoefficient_ =
            lowpassCoefficient(current_.feedbackLowpassHz);
        coefficientLowpassHz_ = current_.feedbackLowpassHz;
    }

    if (current_.feedbackHighpassHz != coefficientHighpassHz_)
    {
        feedbackHighpassCoefficient_ =
            highpassCoefficient(current_.feedbackHighpassHz);
        coefficientHighpassHz_ = current_.feedbackHighpassHz;
    }
}

float RTALStereoDelay::readInterpolated(
    const float* buffer,
    uint32_t writeIndex,
    float delaySamples)
{
    float position =
        static_cast<float>(writeIndex) - delaySamples;

    while (position < 0.0f)
        position += static_cast<float>(bufferSamples_);

    const uint32_t a =
        static_cast<uint32_t>(position) % bufferSamples_;
    const uint32_t b = (a + 1) % bufferSamples_;
    const float fraction = position - floorf(position);

    return buffer[a] + (buffer[b] - buffer[a]) * fraction;
}

void RTALStereoDelay::process(
    float inputLeft,
    float inputRight,
    float& wetLeft,
    float& wetRight)
{
    wetLeft = 0.0f;
    wetRight = 0.0f;

    if (!bufferLeft_ || !bufferRight_) return;

    bool forensicSample = false;
    uint32_t ft = 0;
    if (RTAL_DELAY_FORENSICS_ENABLED)
    {
        forensicSample =
            ((forensicFrameIndex_ % RTAL_DELAY_FORENSICS_SAMPLE_DECIMATION) == 0);
        ++forensicFrameIndex_;
        if (forensicSample) ft = ESP.getCycleCount();
    }

    updateSmoothedParameters();
    updateFilterCoefficientsIfDue();

    if (forensicSample)
    {
        const uint32_t now = ESP.getCycleCount();
        forensics_.smoothCycles += now - ft;
        ft = now;
    }

    const float delaySamplesLeft =
        current_.timeLeftMs * sampleRate_ / 1000.0f;
    const float delaySamplesRight =
        current_.timeRightMs * sampleRate_ / 1000.0f;

    // Build0046l Stable Sync Delay:
    // SYNC tempo changes do not move one read head. Two fixed delay taps are
    // crossfaded, which changes echo timing without Doppler/flanging pitch sweep.
    const bool syncXfade = syncTransitionActive_;
    const float syncMix = syncTransitionMix_;

    // Build0046k Transparent Freeze:
    // Once Freeze has fully crossfaded in, use an integer-delay recirculation
    // path. This avoids repeatedly interpolating the same frozen material,
    // which otherwise behaves like a low-pass over many loop passes.
    const bool transparentFreeze = current_.freeze && freezeMix_ >= 0.9995f;

    float delayedLeft;
    float delayedRight;
    if (syncXfade && !transparentFreeze) {
        const float oldLeftSamples = syncOldLeftMs_ * sampleRate_ / 1000.0f;
        const float oldRightSamples = syncOldRightMs_ * sampleRate_ / 1000.0f;
        const float newLeftSamples = syncNewLeftMs_ * sampleRate_ / 1000.0f;
        const float newRightSamples = syncNewRightMs_ * sampleRate_ / 1000.0f;
        const float oldL = readInterpolated(bufferLeft_, writeIndex_, oldLeftSamples);
        const float oldR = readInterpolated(bufferRight_, writeIndex_, oldRightSamples);
        const float newL = readInterpolated(bufferLeft_, writeIndex_, newLeftSamples);
        const float newR = readInterpolated(bufferRight_, writeIndex_, newRightSamples);
        delayedLeft = oldL + (newL - oldL) * syncMix;
        delayedRight = oldR + (newR - oldR) * syncMix;

        syncTransitionMix_ += syncTransitionStep_;
        if (syncTransitionMix_ >= 1.0f) {
            syncTransitionMix_ = 1.0f;
            syncTransitionActive_ = false;
            current_.timeLeftMs = syncNewLeftMs_;
            current_.timeRightMs = syncNewRightMs_;
        }
    } else if (transparentFreeze) {
        uint32_t dL = static_cast<uint32_t>(delaySamplesLeft + 0.5f);
        uint32_t dR = static_cast<uint32_t>(delaySamplesRight + 0.5f);
        if (dL < 1) dL = 1;
        if (dR < 1) dR = 1;
        if (dL >= bufferSamples_) dL = bufferSamples_ - 1;
        if (dR >= bufferSamples_) dR = bufferSamples_ - 1;
        const uint32_t rL = (writeIndex_ + bufferSamples_ - dL) % bufferSamples_;
        const uint32_t rR = (writeIndex_ + bufferSamples_ - dR) % bufferSamples_;
        delayedLeft = bufferLeft_[rL];
        delayedRight = bufferRight_[rR];
    } else {
        delayedLeft = readInterpolated(bufferLeft_, writeIndex_, delaySamplesLeft);
        delayedRight = readInterpolated(bufferRight_, writeIndex_, delaySamplesRight);
    }

    if (forensicSample)
    {
        const uint32_t now = ESP.getCycleCount();
        forensics_.readCycles += now - ft;
        ft = now;
    }

    const float highpassLeft =
        feedbackHighpassCoefficient_ *
        (feedbackHighpassStateLeft_ +
         delayedLeft - feedbackHighpassInputLeft_);
    const float highpassRight =
        feedbackHighpassCoefficient_ *
        (feedbackHighpassStateRight_ +
         delayedRight - feedbackHighpassInputRight_);

    feedbackHighpassInputLeft_ = delayedLeft;
    feedbackHighpassInputRight_ = delayedRight;
    feedbackHighpassStateLeft_ = highpassLeft;
    feedbackHighpassStateRight_ = highpassRight;

    feedbackFilterStateLeft_ +=
        feedbackLowpassCoefficient_ *
        (highpassLeft - feedbackFilterStateLeft_);
    feedbackFilterStateRight_ +=
        feedbackLowpassCoefficient_ *
        (highpassRight - feedbackFilterStateRight_);

    const float saturatedLeft =
        saturateFeedback(
            feedbackFilterStateLeft_,
            current_.feedbackSaturation);
    const float saturatedRight =
        saturateFeedback(
            feedbackFilterStateRight_,
            current_.feedbackSaturation);

    const float directGain = 1.0f - current_.crossfeed;
    const float crossGain = current_.crossfeed;

    const float feedbackToLeft =
        saturatedLeft * directGain +
        saturatedRight * crossGain;

    const float feedbackToRight =
        saturatedRight * directGain +
        saturatedLeft * crossGain;

    // At full Freeze the stored stereo loop must not be re-filtered,
    // re-saturated or re-crossfed on every circulation. Doing so progressively
    // darkens/changes the captured signal even with LPF near 18 kHz.
    const float loopToLeft = transparentFreeze ? delayedLeft : feedbackToLeft;
    const float loopToRight = transparentFreeze ? delayedRight : feedbackToRight;

    if (forensicSample)
    {
        const uint32_t now = ESP.getCycleCount();
        forensics_.feedbackCycles += now - ft;
        ft = now;
    }

    const float inputEnvelope = fmaxf(fabsf(inputLeft), fabsf(inputRight));

    if (inputEnvelope > duckEnvelope_)
        duckEnvelope_ = inputEnvelope;
    else
        duckEnvelope_ = inputEnvelope + duckReleaseCoefficient_ * (duckEnvelope_ - inputEnvelope);

    const float duckThreshold = duckThresholdLinear_;
    float duckTarget = 1.0f;

    if (current_.duckAmount > 0.0001f && duckEnvelope_ > duckThreshold)
    {
        const float normalizedOver = clamp(
            (duckEnvelope_ - duckThreshold) / (1.0f - duckThreshold),
            0.0f, 1.0f);
        duckTarget = 1.0f - current_.duckAmount * normalizedOver;
    }

    if (duckTarget < duckGain_)
        duckGain_ = duckTarget;
    else
        duckGain_ = duckTarget + duckReleaseCoefficient_ * (duckGain_ - duckTarget);

    const float freezeTarget = current_.freeze ? 1.0f : 0.0f;
    freezeMix_ +=
        (freezeTarget - freezeMix_) * RTAL_DELAY_FREEZE_SMOOTHING;

    const float freezeInputGain = 1.0f - freezeMix_;
    const float freezeFeedback =
        current_.feedback + (1.0f - current_.feedback) * freezeMix_;

    if (forensicSample)
    {
        const uint32_t now = ESP.getCycleCount();
        forensics_.duckCycles += now - ft;
        ft = now;
    }

    if (current_.enabled)
    {
        bufferLeft_[writeIndex_] =
            inputLeft * freezeInputGain +
            loopToLeft * freezeFeedback;
        bufferRight_[writeIndex_] =
            inputRight * freezeInputGain +
            loopToRight * freezeFeedback;

        wetLeft = delayedLeft * current_.level * duckGain_;
        wetRight = delayedRight * current_.level * duckGain_;
    }
    else
    {
        bufferLeft_[writeIndex_] = inputLeft;
        bufferRight_[writeIndex_] = inputRight;
    }

    if (forensicSample)
    {
        const uint32_t now = ESP.getCycleCount();
        forensics_.writeCycles += now - ft;
        ft = now;
    }

    const float wetPeakLeft = fabsf(wetLeft);
    const float wetPeakRight = fabsf(wetRight);
    const float feedbackPeakLeft = fabsf(feedbackToLeft);
    const float feedbackPeakRight = fabsf(feedbackToRight);

    portENTER_CRITICAL(&mux_);
    ++statistics_.processedFrames;
    if (wetPeakLeft > statistics_.peakWetLeft)
        statistics_.peakWetLeft = wetPeakLeft;
    if (wetPeakRight > statistics_.peakWetRight)
        statistics_.peakWetRight = wetPeakRight;
    if (feedbackPeakLeft > statistics_.peakFeedbackLeft)
        statistics_.peakFeedbackLeft = feedbackPeakLeft;
    if (feedbackPeakRight > statistics_.peakFeedbackRight)
        statistics_.peakFeedbackRight = feedbackPeakRight;
    portEXIT_CRITICAL(&mux_);

    ++writeIndex_;
    if (writeIndex_ >= bufferSamples_)
        writeIndex_ = 0;

    if (forensicSample)
    {
        forensics_.statsCycles += ESP.getCycleCount() - ft;
        ++forensics_.sampledFrames;
    }
}

void RTALStereoDelay::endBlock()
{
    if (!RTAL_DELAY_FORENSICS_ENABLED) return;

    portENTER_CRITICAL(&mux_);
    forensicWindow_.sampledFrames += forensics_.sampledFrames;
    forensicWindow_.totalFrames += forensics_.totalFrames;
    forensicWindow_.smoothCycles += forensics_.smoothCycles;
    forensicWindow_.readCycles += forensics_.readCycles;
    forensicWindow_.feedbackCycles += forensics_.feedbackCycles;
    forensicWindow_.duckCycles += forensics_.duckCycles;
    forensicWindow_.writeCycles += forensics_.writeCycles;
    forensicWindow_.statsCycles += forensics_.statsCycles;
    portEXIT_CRITICAL(&mux_);
}

RTALStereoDelayForensics RTALStereoDelay::forensics()
{
    return forensics_;
}

RTALStereoDelayForensics RTALStereoDelay::forensicWindow(bool resetWindow)
{
    portENTER_CRITICAL(&mux_);
    const RTALStereoDelayForensics copy = forensicWindow_;
    if (resetWindow) forensicWindow_ = {};
    portEXIT_CRITICAL(&mux_);
    return copy;
}

RTALStereoDelayStatistics RTALStereoDelay::statistics(bool resetWindow)
{
    portENTER_CRITICAL(&mux_);
    const RTALStereoDelayStatistics copy = statistics_;

    if (resetWindow)
    {
        const size_t bytes = statistics_.allocatedBytes;
        const bool psram = statistics_.usingPsram;
        statistics_ = {};
        statistics_.allocatedBytes = bytes;
        statistics_.usingPsram = psram;
    }

    portEXIT_CRITICAL(&mux_);
    return copy;
}

void RTALStereoDelay::printReport()
{
    const RTALStereoDelayStatistics s = statistics(true);
    const RTALStereoDelayForensics f = forensicWindow(true);
    const RTALStereoDelayParameters p = parameters();

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Delay enabled=%s crossfeed=%.3f L=%.1fms R=%.1fms feedback=%.3f SAT=%.2f HPF=%.0fHz LPF=%.0fHz level=%.3f",
        p.enabled ? "ON" : "OFF",
        p.crossfeed,
        p.timeLeftMs,
        p.timeRightMs,
        p.feedback,
        p.feedbackSaturation,
        p.feedbackHighpassHz,
        p.feedbackLowpassHz,
        p.level);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Delay frames=%llu blocks=%lu peak_wet=%.5f/%.5f peak_fb=%.5f/%.5f bytes=%lu mem=%s",
        static_cast<unsigned long long>(s.processedFrames),
        static_cast<unsigned long>(s.processedBlocks),
        s.peakWetLeft,
        s.peakWetRight,
        s.peakFeedbackLeft,
        s.peakFeedbackRight,
        static_cast<unsigned long>(s.allocatedBytes),
        s.usingPsram ? "PSRAM" : "UNKNOWN");

    if (RTAL_DELAY_FORENSICS_ENABLED && f.sampledFrames)
    {
        const auto avgFrameUs = [&f](uint64_t cycles) -> float
        {
            return static_cast<float>(cycles) /
                (static_cast<float>(RTAL_CPU_CYCLES_PER_US) * f.sampledFrames);
        };
        const float scale = static_cast<float>(RTAL_AUDIO_BLOCK_FRAMES);
        RTALLogger::printf(
            RTALLogLevel::Info,
            "DelayDiag sample=1/%lu n=%lu est_block smooth=%.0f read=%.0f feedback=%.0f duck=%.0f write=%.0f stats=%.0f us",
            static_cast<unsigned long>(RTAL_DELAY_FORENSICS_SAMPLE_DECIMATION),
            static_cast<unsigned long>(f.sampledFrames),
            avgFrameUs(f.smoothCycles) * scale,
            avgFrameUs(f.readCycles) * scale,
            avgFrameUs(f.feedbackCycles) * scale,
            avgFrameUs(f.duckCycles) * scale,
            avgFrameUs(f.writeCycles) * scale,
            avgFrameUs(f.statsCycles) * scale);
    }
}
