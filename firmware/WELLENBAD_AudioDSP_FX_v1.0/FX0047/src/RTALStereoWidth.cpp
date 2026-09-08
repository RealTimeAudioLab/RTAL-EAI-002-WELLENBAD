#include "../include/RTALStereoWidth.h"

portMUX_TYPE RTALStereoWidth::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALStereoWidthParameters RTALStereoWidth::target_ = { true, 1.0f };
bool RTALStereoWidth::blockEnabled_ = true;
bool RTALStereoWidth::blockNeedsProcessing_ = false;
float RTALStereoWidth::currentWidth_ = 1.0f;
float RTALStereoWidth::widthStep_ = 0.0f;

float RTALStereoWidth::clampWidth(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 2.0f) return 2.0f;
    return v;
}

RTALStatus RTALStereoWidth::begin() {
    portENTER_CRITICAL(&mux_);
    target_.enabled = true;
    target_.width = 1.0f;
    blockEnabled_ = target_.enabled;
    blockNeedsProcessing_ = false;
    currentWidth_ = target_.width;
    widthStep_ = 0.0f;
    portEXIT_CRITICAL(&mux_);
    return RTALStatus::OK;
}

void RTALStereoWidth::setEnabled(bool enabled) {
    portENTER_CRITICAL(&mux_); target_.enabled = enabled; portEXIT_CRITICAL(&mux_);
}
void RTALStereoWidth::setWidth(float width) {
    portENTER_CRITICAL(&mux_); target_.width = clampWidth(width); portEXIT_CRITICAL(&mux_);
}
RTALStereoWidthParameters RTALStereoWidth::parameters() {
    portENTER_CRITICAL(&mux_); const auto p = target_; portEXIT_CRITICAL(&mux_); return p;
}
void RTALStereoWidth::beginBlock(size_t frames) {
    RTALStereoWidthParameters p;
    portENTER_CRITICAL(&mux_); p = target_; portEXIT_CRITICAL(&mux_);
    blockEnabled_ = p.enabled;
    if (!blockEnabled_) {
        currentWidth_ = p.width;
        widthStep_ = 0.0f;
        blockNeedsProcessing_ = false;
        return;
    }
    const float n = frames ? (float)frames : 1.0f;
    widthStep_ = (p.width - currentWidth_) / n;
    constexpr float kUnityEpsilon = 0.0001f;
    blockNeedsProcessing_ =
        (fabsf(currentWidth_ - 1.0f) > kUnityEpsilon) ||
        (fabsf(p.width - 1.0f) > kUnityEpsilon);
}

bool RTALStereoWidth::activeForBlock() {
    return blockEnabled_ && blockNeedsProcessing_;
}
