#include "DisplayUI.h"
#include <Wire.h>
#include <math.h>
#include <cstring>
#include <cstdio>

DisplayUI::DisplayUI()
  : display_(hw::SCREEN_WIDTH, hw::SCREEN_HEIGHT, &Wire, hw::OLED_RESET_PIN) {}

bool DisplayUI::begin(uint8_t contrast) {
  Wire.begin();
  Wire.setClock(hw::I2C_CLOCK_HZ);
#if OLED_USE_SH1106
  if (!display_.begin(hw::OLED_ADDRESS, true)) return false;
#else
  if (!display_.begin(SSD1306_SWITCHCAPVCC, hw::OLED_ADDRESS)) return false;
#endif
  display_.clearDisplay();
  display_.setTextColor(OLED_WHITE);
  setContrast(contrast);
  display_.display();
  return true;
}

void DisplayUI::setFlipped(bool flipped) {
  display_.setRotation(flipped ? 2 : 0);
  display_.clearDisplay();
  display_.display();
}

void DisplayUI::setAnimationTuning(uint8_t idleSpeed, uint8_t heartParticles) {
  idleAnimationSpeed_ = constrain(idleSpeed, 50, 150);
  heartParticleCount_ = constrain(heartParticles, 0, 16);
}

void DisplayUI::setContrast(uint8_t contrast) {
#if OLED_USE_SH1106
  display_.setContrast(contrast);
#else
  display_.ssd1306_command(SSD1306_SETCONTRAST);
  display_.ssd1306_command(contrast);
#endif
}

void DisplayUI::centerText(const char* text, int16_t y, uint8_t size) {
  display_.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  display_.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  (void)x1;
  (void)y1;
  (void)h;
  display_.setCursor((hw::SCREEN_WIDTH - static_cast<int16_t>(w)) / 2, y);
  display_.print(text);
}

void DisplayUI::drawEye(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t radius) {
  if (w < 2) w = 2;
  if (h < 2) h = 2;
  const int16_t smallest = (w < h) ? w : h;
  const int16_t maxRadius = smallest / 2;
  if (radius > maxRadius) radius = maxRadius;
  display_.fillRoundRect(cx - w / 2, cy - h / 2, w, h, radius, OLED_WHITE);
}

void DisplayUI::drawBrowArc(int16_t cx, int16_t cy, int16_t halfWidth, int8_t tilt, int8_t arch, uint8_t thickness) {
  // Thick quadratic arc with round caps. Positive arch lifts the middle of the brow.
  const int16_t x0 = cx - halfWidth;
  const int16_t x2 = cx + halfWidth;
  const int16_t y0 = cy - tilt;
  const int16_t y2 = cy + tilt;
  const int16_t x1 = cx;
  const int16_t y1 = cy - arch;
  const uint8_t radius = (thickness / 2 > 2) ? (thickness / 2) : 2;

  constexpr uint8_t steps = 28;
  for (uint8_t i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    const float u = 1.0f - t;
    const float x = u * u * x0 + 2.0f * u * t * x1 + t * t * x2;
    const float y = u * u * y0 + 2.0f * u * t * y1 + t * t * y2;
    display_.fillCircle(static_cast<int16_t>(x + 0.5f), static_cast<int16_t>(y + 0.5f), radius, OLED_WHITE);
  }
}

void DisplayUI::drawHeartEye(int16_t cx, int16_t cy, uint8_t size) {
  // Rasterize the classic implicit heart curve. It stays symmetric and clean
  // at every size, unlike the old circle+triangle approximation.
  const int16_t radius = max<int16_t>(2, size / 2);
  const float sx = static_cast<float>(radius) * 0.92f;
  const float sy = static_cast<float>(radius) * 0.92f;
  for (int16_t py = -radius; py <= radius + 2; ++py) {
    for (int16_t px = -radius - 1; px <= radius + 1; ++px) {
      const float x = static_cast<float>(px) / sx;
      const float y = -static_cast<float>(py - 1) / sy;
      const float a = x * x + y * y - 1.0f;
      if ((a * a * a - x * x * y * y * y) <= 0.0f) {
        display_.drawPixel(cx + px, cy + py, OLED_WHITE);
      }
    }
  }
}

void DisplayUI::drawXEye(int16_t cx, int16_t cy, int16_t w, int16_t h, uint8_t thickness) {
  const int16_t hw = max<int16_t>(4, w / 2);
  const int16_t hh = max<int16_t>(4, h / 2);
  const int16_t x0 = cx - hw;
  const int16_t x1 = cx + hw;
  const int16_t y0 = cy - hh;
  const int16_t y1 = cy + hh;
  const int16_t half = max<int16_t>(0, static_cast<int16_t>(thickness) / 2);
  for (int16_t o = -half; o <= half; ++o) {
    display_.drawLine(x0, y0 + o, x1, y1 + o, OLED_WHITE);
    display_.drawLine(x0, y1 + o, x1, y0 + o, OLED_WHITE);
  }
  // Rounded-ish endpoints keep the X consistent with the pet's soft eyebrows.
  const uint8_t r = max<uint8_t>(1, thickness / 2);
  display_.fillCircle(x0, y0, r, OLED_WHITE);
  display_.fillCircle(x1, y1, r, OLED_WHITE);
  display_.fillCircle(x0, y1, r, OLED_WHITE);
  display_.fillCircle(x1, y0, r, OLED_WHITE);
}

void DisplayUI::drawHeartParticles(uint32_t now, float envelope) {
  if (envelope <= 0.02f) return;
  // Deterministic particles: no random flicker. Each heart rises, drifts and
  // loops at a different phase around the face.
  static const int8_t baseX[] = {-48,-35,-19, 20, 36, 49,-55, 54, -6, 8,-43, 43,-27, 28,-12, 14};
  static const int8_t baseY[] = { 18,  7, 25,  9, 24, 13, 34, 36,  4,30, 28, 31, 13, 18, 38, 41};
  static const uint8_t phase[] = {0,31,67,19,83,46,58,11,74,92,23,53,88,39,64, 7};
  const uint8_t available = static_cast<uint8_t>(sizeof(baseX) / sizeof(baseX[0]));
  const uint8_t count = heartParticleCount_ < available ? heartParticleCount_ : available;
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t cycle = static_cast<uint8_t>(((now / 22UL) + phase[i]) % 100UL);
    const float life = static_cast<float>(cycle) / 100.0f;
    if (life < 0.10f || life > 0.94f) continue;
    const float fade = min(life / 0.20f, (1.0f - life) / 0.16f);
    if (fade * envelope < 0.26f) continue;
    const int16_t x = 64 + baseX[i] + static_cast<int16_t>(sinf(life * 6.28318f + i) * 3.0f);
    const int16_t y = 61 - baseY[i] - static_cast<int16_t>(life * 24.0f);
    const uint8_t size = static_cast<uint8_t>(4 + (i % 3) + envelope * 2.0f);
    if (x > 3 && x < 124 && y > 4 && y < 61) drawHeartEye(x, y, size);
  }
}

void DisplayUI::drawTimerLayer(const char* text, int16_t y, uint8_t alphaStep, int16_t xOffset) {
  if (alphaStep == 0) return;

  const uint8_t size = 2;
  const int16_t charWidth = 6 * size;
  const int16_t width = static_cast<int16_t>(strlen(text)) * charWidth;
  const int16_t startX = (hw::SCREEN_WIDTH - width) / 2 + xOffset;

  // 4x4 Bayer pattern: fake opacity on a 1-bit OLED.
  static const uint8_t bayer[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5}
  };

  GFXcanvas1 canvas(width > 0 ? width : 1, 16);
  canvas.fillScreen(0);
  canvas.setTextColor(1);
  canvas.setTextSize(size);
  canvas.setCursor(0, 0);
  canvas.print(text);

  const uint8_t threshold = min<uint8_t>(16, alphaStep);
  for (int16_t py = 0; py < canvas.height(); ++py) {
    const int16_t dy = y + py;
    if (dy < 14 || dy >= 45) continue;
    for (int16_t px = 0; px < canvas.width(); ++px) {
      if (!canvas.getPixel(px, py)) continue;
      const int16_t dx = startX + px;
      if (dx < 0 || dx >= hw::SCREEN_WIDTH) continue;
      if (bayer[dy & 3][dx & 3] < threshold) display_.drawPixel(dx, dy, OLED_WHITE);
    }
  }
}

void DisplayUI::drawSmallTextLayer(const char* text, int16_t x, int16_t y, uint8_t alphaStep) {
  if (alphaStep == 0 || !text) return;

  static const uint8_t bayer[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5}
  };

  const int16_t width = max<int16_t>(1, static_cast<int16_t>(strlen(text) * 6));
  GFXcanvas1 canvas(width, 8);
  canvas.fillScreen(0);
  canvas.setTextColor(1);
  canvas.setTextSize(1);
  canvas.setCursor(0, 0);
  canvas.print(text);

  const uint8_t threshold = min<uint8_t>(16, alphaStep);
  for (int16_t py = 0; py < canvas.height(); ++py) {
    const int16_t dy = y + py;
    if (dy < 0 || dy >= hw::SCREEN_HEIGHT) continue;
    for (int16_t px = 0; px < canvas.width(); ++px) {
      if (!canvas.getPixel(px, py)) continue;
      const int16_t dx = x + px;
      if (dx < 0 || dx >= hw::SCREEN_WIDTH) continue;
      if (bayer[dy & 3][dx & 3] < threshold) display_.drawPixel(dx, dy, OLED_WHITE);
    }
  }
}

void DisplayUI::drawTimerGlyph(char glyph, int16_t x, int16_t y, uint8_t alphaStep) {
  if (alphaStep == 0) return;

  static const uint8_t bayer[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5}
  };

  GFXcanvas1 canvas(12, 16);
  canvas.fillScreen(0);
  canvas.setTextColor(1);
  canvas.setTextSize(2);
  canvas.setCursor(0, 0);
  canvas.print(glyph);

  const uint8_t threshold = min<uint8_t>(16, alphaStep);
  for (int16_t py = 0; py < canvas.height(); ++py) {
    const int16_t dy = y + py;
    if (dy < 14 || dy >= 45) continue;
    for (int16_t px = 0; px < canvas.width(); ++px) {
      if (!canvas.getPixel(px, py)) continue;
      const int16_t dx = x + px;
      if (dx < 0 || dx >= hw::SCREEN_WIDTH) continue;
      if (bayer[dy & 3][dx & 3] < threshold) display_.drawPixel(dx, dy, OLED_WHITE);
    }
  }
}

void DisplayUI::drawSparkle(int16_t x, int16_t y, uint8_t size) {
  display_.drawLine(x - size, y, x + size, y, OLED_WHITE);
  display_.drawLine(x, y - size, x, y + size, OLED_WHITE);
}

void DisplayUI::drawStyledEye(PetEyeStyle style, int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t radius, bool leftEye, uint32_t now) {
  if (w < 3) w = 3;
  if (h < 3) h = 3;
  switch (style) {
    case PetEyeStyle::Square:
      display_.fillRoundRect(cx - w / 2, cy - h / 2, w, h, min<int16_t>(radius, 3), OLED_WHITE);
      break;
    case PetEyeStyle::Pill:
      display_.fillRoundRect(cx - w / 2, cy - h / 2, w, h, h / 2, OLED_WHITE);
      break;
    case PetEyeStyle::Pixel: {
      const int16_t x = cx - w / 2;
      const int16_t y = cy - h / 2;
      const int16_t cut = 4;
      display_.fillRect(x + cut, y, w - 2 * cut, h, OLED_WHITE);
      display_.fillRect(x, y + cut, w, h - 2 * cut, OLED_WHITE);
      const int16_t scanY = y + 4 + static_cast<int16_t>((now / 140UL) % max<int16_t>(1, h - 8));
      display_.fillRect(x + 5, scanY, max<int16_t>(2, w - 10), 1, 0);
      break;
    }
    case PetEyeStyle::Alien: {
      const int16_t slant = leftEye ? 5 : -5;
      display_.fillTriangle(cx - w / 2, cy - h / 4 + slant / 2,
                            cx + w / 2, cy - h / 2 - slant / 2,
                            cx + w / 3, cy + h / 2, OLED_WHITE);
      display_.fillTriangle(cx - w / 2, cy - h / 4 + slant / 2,
                            cx - w / 3, cy + h / 2,
                            cx + w / 3, cy + h / 2, OLED_WHITE);
      const int16_t slitW = max<int16_t>(4, w / 3);
      display_.fillRoundRect(cx - slitW / 2, cy - 1, slitW, 3, 1, 0);
      break;
    }
    case PetEyeStyle::Cat: {
      display_.fillTriangle(cx - w / 2, cy,
                            cx, cy - h / 2,
                            cx + w / 2, cy, OLED_WHITE);
      display_.fillTriangle(cx - w / 2, cy,
                            cx, cy + h / 2,
                            cx + w / 2, cy, OLED_WHITE);
      const int16_t slitH = max<int16_t>(3, h / 2);
      display_.fillRoundRect(cx - 1, cy - slitH / 2, 3, slitH, 1, 0);
      break;
    }
    case PetEyeStyle::Visor: {
      display_.fillRoundRect(cx - w / 2, cy - h / 2, w, h, h / 2, OLED_WHITE);
      display_.fillRoundRect(cx - w / 2 + 3, cy - h / 2 + 3, w - 6, max<int16_t>(2, h - 6), max<int16_t>(1, h / 3), 0);
      const int16_t inner = max<int16_t>(2, w - 10);
      const int16_t scanX = cx - inner / 2 + static_cast<int16_t>((now / 75UL) % inner);
      display_.fillRoundRect(scanX - 1, cy - 3, 3, 7, 1, OLED_WHITE);
      break;
    }
    case PetEyeStyle::Ring: {
      const int16_t r = min<int16_t>(w, h) / 2;
      display_.fillCircle(cx, cy, r, OLED_WHITE);
      display_.fillCircle(cx, cy, max<int16_t>(1, r - 4), 0);
      const int16_t pupil = max<int16_t>(2, r / 3);
      display_.fillCircle(cx, cy, pupil, OLED_WHITE);
      const float a = static_cast<float>(now % 1600UL) / 1600.0f * 6.283185f + (leftEye ? 0.0f : 3.14159f);
      display_.fillCircle(cx + static_cast<int16_t>(cosf(a) * (r + 2)), cy + static_cast<int16_t>(sinf(a) * (r + 2)), 1, OLED_WHITE);
      break;
    }
    case PetEyeStyle::Sleep:
      display_.fillRoundRect(cx - w / 2, cy - max<int16_t>(2, h / 4), w, max<int16_t>(4, h / 2), max<int16_t>(2, h / 4), OLED_WHITE);
      break;
    case PetEyeStyle::Scout: {
      display_.drawRoundRect(cx - w / 2, cy - h / 2, w, h, min<int16_t>(radius, 8), OLED_WHITE);
      display_.drawRoundRect(cx - w / 2 + 2, cy - h / 2 + 2, w - 4, h - 4, min<int16_t>(radius, 7), OLED_WHITE);
      const int16_t pr = max<int16_t>(2, min<int16_t>(w, h) / 5);
      const int8_t probe = static_cast<int8_t>(((now / 420UL) % 3) - 1);
      display_.fillCircle(cx + probe, cy, pr, OLED_WHITE);
      break;
    }
    case PetEyeStyle::Bubble: {
      const int16_t r = min<int16_t>(w, h) / 2;
      display_.fillCircle(cx, cy, r, OLED_WHITE);
      display_.fillCircle(cx + (leftEye ? 3 : -3), cy + 2, max<int16_t>(2, r / 3), 0);
      display_.fillCircle(cx + (leftEye ? -4 : 4), cy - 5, 2, 0);
      break;
    }
    case PetEyeStyle::Mask: {
      const int16_t dir = leftEye ? 1 : -1;
      display_.fillTriangle(cx - w / 2, cy - h / 3,
                            cx + w / 2, cy - h / 2 + dir * 2,
                            cx + dir * (w / 4), cy + h / 2, OLED_WHITE);
      display_.fillTriangle(cx - w / 2, cy - h / 3,
                            cx - dir * (w / 4), cy + h / 2,
                            cx + dir * (w / 4), cy + h / 2, OLED_WHITE);
      break;
    }
    case PetEyeStyle::Rounded:
    default:
      drawEye(cx, cy, w, h, radius);
      break;
  }
}

void DisplayUI::drawStyledBrow(PetBrowStyle style, int16_t cx, int16_t cy, int16_t halfWidth, int8_t tilt, int8_t arch, uint8_t thickness) {
  switch (style) {
    case PetBrowStyle::Flat:
      drawBrowArc(cx, cy, halfWidth, tilt, 1, thickness);
      break;
    case PetBrowStyle::Short:
      drawBrowArc(cx, cy, max<int16_t>(7, halfWidth - 4), tilt, max<int8_t>(2, arch - 2), thickness);
      break;
    case PetBrowStyle::Floating:
      drawBrowArc(cx, cy - 2, max<int16_t>(8, halfWidth - 2), tilt, arch + 2, max<uint8_t>(4, thickness - 1));
      break;
    case PetBrowStyle::Arc:
    default:
      drawBrowArc(cx, cy, halfWidth, tilt, arch, thickness);
      break;
  }
}

void DisplayUI::drawFace(PetMood mood, uint32_t now, float p, uint8_t personality, float pressAmount, const PetMotionInput& motionInput) {
  const PetSkinDefinition& skin = petSkinDefinition(personality);
  PetExpression expr = resolvePetExpression(mood, now, p);
  const PetMotionPose motion = resolvePetMotion(motionInput, now);

  int16_t leftX = skin.leftX;
  int16_t rightX = skin.rightX;
  int16_t eyeY = skin.eyeY;
  int16_t eyeW = skin.eyeW;
  int16_t eyeH = skin.eyeH;

  pressAmount = constrain(pressAmount, 0.0f, 1.0f);
  const float pressEase = 1.0f - powf(1.0f - pressAmount, 3.0f);
  const int8_t pressDrop = static_cast<int8_t>(pressEase * 5.0f + 0.5f);
  eyeY += pressDrop;
  eyeW += static_cast<int8_t>(pressEase * 3.0f + 0.5f);
  eyeH = max<int16_t>(7, eyeH - static_cast<int8_t>(pressEase * 6.0f + 0.5f));

  // Slow breathing remains skin-independent, so every design feels like the same pet.
  const uint32_t animNow = static_cast<uint32_t>((static_cast<uint64_t>(now) * idleAnimationSpeed_) / 100ULL);
  eyeY += ((animNow / 320UL) & 1) ? 1 : 0;

  // Idle glance is an emotion-level micro animation; MPU/touch gravity is layered after it.
  if (mood == PetMood::Idle && fabsf(motionInput.tiltX) < 0.12f && fabsf(motionInput.tiltY) < 0.12f) {
    const uint8_t gazePhase = (animNow / 1700UL) % 5;
    const int8_t gaze = gazePhase == 1 ? -2 : (gazePhase == 3 ? 2 : 0);
    leftX += gaze;
    rightX += gaze;
  }

  leftX += expr.leftX;
  rightX += expr.rightX;
  int16_t leftY = eyeY + expr.leftY;
  int16_t rightY = eyeY + expr.rightY;

  // Gravity/inertia layer. On a side, both eyes fall toward the lower side and
  // move closer together. The eye closest to the edge is compressed slightly more.
  leftX += static_cast<int16_t>(roundf(motion.shiftX + motion.wobbleX));
  rightX += static_cast<int16_t>(roundf(motion.shiftX + motion.wobbleX));
  leftY += static_cast<int16_t>(roundf(motion.shiftY + motion.wobbleY));
  rightY += static_cast<int16_t>(roundf(motion.shiftY + motion.wobbleY));

  // Rotational inertia makes the pair behave like a tiny cartoon head instead
  // of two independent UI widgets: one side rises while the other falls.
  const int16_t headTilt = static_cast<int16_t>(roundf(motion.cartoonTilt));
  leftY -= headTilt;
  rightY += headTilt;
  if (motion.sideAmount > 0.0f) {
    const int16_t cluster = static_cast<int16_t>(motion.cluster + 0.5f);
    if (motionInput.tiltX < 0.0f) {
      rightX -= cluster;
      leftX -= static_cast<int16_t>(cluster * 0.35f);
    } else {
      leftX += cluster;
      rightX += static_cast<int16_t>(cluster * 0.35f);
    }
  }

  int16_t leftW = max<int16_t>(4, static_cast<int16_t>(eyeW * expr.leftScaleX * motion.squashX));
  int16_t rightW = max<int16_t>(4, static_cast<int16_t>(eyeW * expr.rightScaleX * motion.squashX));
  int16_t leftH = max<int16_t>(3, static_cast<int16_t>(eyeH * expr.leftOpen * motion.squashY));
  int16_t rightH = max<int16_t>(3, static_cast<int16_t>(eyeH * expr.rightOpen * motion.squashY));

  if (motion.sideAmount > 0.55f) {
    const float edgeSquash = 1.0f - 0.22f * motion.sideAmount;
    if (motionInput.tiltX < 0.0f) leftW = max<int16_t>(4, static_cast<int16_t>(leftW * edgeSquash));
    else rightW = max<int16_t>(4, static_cast<int16_t>(rightW * edgeSquash));
  }

  if (expr.jitter) {
    const int8_t wobble = ((now / 80UL) & 1) ? 3 : -3;
    leftY += wobble;
    rightY -= wobble;
  }

  if (expr.xEyes) {
    const uint8_t xThickness = max<uint8_t>(3, skin.browThickness / 2);
    drawXEye(leftX, leftY, max<int16_t>(12, leftW), max<int16_t>(12, leftH), xThickness);
    drawXEye(rightX, rightY, max<int16_t>(12, rightW), max<int16_t>(12, rightH), xThickness);
  } else if (expr.hearts) {
    const float intro = min(1.0f, p * 6.5f);
    const float outro = p > 0.78f ? constrain((1.0f - p) / 0.22f, 0.0f, 1.0f) : 1.0f;
    const float envelope = min(intro, outro);
    const float pulse = 0.5f + 0.5f * sinf(static_cast<float>(now) * 0.014f);
    const uint8_t heartSize = static_cast<uint8_t>(5.0f + envelope * 13.0f + envelope * pulse * 2.0f);
    const int8_t bounce = ((now / 120UL) & 1) ? -1 : 1;
    drawHeartEye(leftX, leftY + bounce, heartSize);
    drawHeartEye(rightX, rightY - bounce, heartSize);
    drawHeartParticles(now, envelope);
  } else {
    // VISOR gets a small bridge, MASK a subtle outer frame. These are skin details,
    // not emotions, which keeps the emotion system reusable across every design.
    if (skin.eyeStyle == PetEyeStyle::Visor) {
      display_.drawLine(leftX + leftW / 2 - 2, eyeY, rightX - rightW / 2 + 2, eyeY, OLED_WHITE);
    } else if (skin.eyeStyle == PetEyeStyle::Mask) {
      display_.drawLine(14, 24, 25, 18, OLED_WHITE);
      display_.drawLine(103, 18, 114, 24, OLED_WHITE);
      display_.drawLine(18, 51, 28, 56, OLED_WHITE);
      display_.drawLine(100, 56, 110, 51, OLED_WHITE);
    } else if (skin.eyeStyle == PetEyeStyle::Cat) {
      display_.drawLine(21, 18, 28, 27, OLED_WHITE);
      display_.drawLine(107, 18, 100, 27, OLED_WHITE);
    } else if (skin.eyeStyle == PetEyeStyle::Pixel) {
      display_.fillRect(8, 19, 3, 3, OLED_WHITE);
      display_.fillRect(117, 19, 3, 3, OLED_WHITE);
      display_.fillRect(8, 52, 3, 3, OLED_WHITE);
      display_.fillRect(117, 52, 3, 3, OLED_WHITE);
    } else if (skin.eyeStyle == PetEyeStyle::Alien) {
      display_.fillCircle(64, 8, 2, OLED_WHITE);
      display_.drawLine(64, 10, 64, 14, OLED_WHITE);
    } else if (skin.eyeStyle == PetEyeStyle::Scout) {
      display_.drawLine(5, 31, 10, 31, OLED_WHITE);
      display_.drawLine(118, 31, 123, 31, OLED_WHITE);
      display_.drawLine(64, 54, 64, 59, OLED_WHITE);
    } else if (skin.eyeStyle == PetEyeStyle::Bubble) {
      display_.drawCircle(12, 48, 3, OLED_WHITE);
      display_.drawCircle(116, 20, 2, OLED_WHITE);
    }
    drawStyledEye(skin.eyeStyle, leftX, leftY, leftW, leftH, skin.radius, true, now);
    drawStyledEye(skin.eyeStyle, rightX, rightY, rightW, rightH, skin.radius, false, now);
  }

  const int16_t browHalf = max<int16_t>(10, eyeW / 2);
  int16_t browY = 13 + expr.browY + static_cast<int16_t>(pressDrop * 0.65f) +
                  static_cast<int16_t>((motion.shiftY + motion.wobbleY) * 0.35f);
  int8_t leftTilt = expr.leftBrowTilt;
  int8_t rightTilt = expr.rightBrowTilt;
  const int8_t inertiaTilt = static_cast<int8_t>(constrain(motion.cartoonTilt * 0.8f, -4.0f, 4.0f));
  leftTilt += inertiaTilt;
  rightTilt += inertiaTilt;
  if (motion.sideAmount > 0.5f) {
    const int8_t gravityTilt = static_cast<int8_t>(motionInput.tiltX * 3.0f);
    leftTilt += gravityTilt;
    rightTilt += gravityTilt;
  }
  drawStyledBrow(skin.browStyle, leftX, browY - headTilt, browHalf, leftTilt, expr.leftBrowArch, skin.browThickness);
  drawStyledBrow(skin.browStyle, rightX, browY + headTilt, browHalf, rightTilt, expr.rightBrowArch, skin.browThickness);

  if (expr.sparkle) {
    const uint8_t s = ((now / 170UL) & 1) ? 2 : 4;
    drawSparkle(10, 34, s);
    drawSparkle(118, 29, max<uint8_t>(2, s - 1));
  }
  if (motion.faceDown && mood == PetMood::Idle) {
    // Tiny visual hint when the unit is flipped over; no text needed.
    display_.drawLine(58, 60, 70, 60, OLED_WHITE);
  }
}

void DisplayUI::drawAccessory(uint8_t accessory, uint32_t now) {
  switch (accessory) {
    case 1: { // Spark
      const uint8_t s = ((now / 220) % 2) ? 2 : 4;
      drawSparkle(114, 12, s);
      break;
    }
    case 2: { // Orbit
      for (uint8_t i = 0; i < 10; ++i) {
        const float a = (static_cast<float>(i) / 10.0f) * 6.283185f;
        const int16_t x = 64 + static_cast<int16_t>(59.0f * cosf(a));
        const int16_t y = 34 + static_cast<int16_t>(27.0f * sinf(a));
        display_.drawPixel(x, y, OLED_WHITE);
      }
      const float a = static_cast<float>(now % 2400) / 2400.0f * 6.283185f;
      const int16_t x = 64 + static_cast<int16_t>(59.0f * cosf(a));
      const int16_t y = 34 + static_cast<int16_t>(27.0f * sinf(a));
      display_.fillCircle(x, y, 2, OLED_WHITE);
      break;
    }
    case 3: { // Crown, deliberately above the face
      display_.drawLine(52, 6, 56, 1, OLED_WHITE);
      display_.drawLine(56, 1, 62, 6, OLED_WHITE);
      display_.drawLine(62, 6, 68, 1, OLED_WHITE);
      display_.drawLine(68, 1, 75, 6, OLED_WHITE);
      display_.drawLine(52, 6, 75, 6, OLED_WHITE);
      break;
    }
    default:
      break;
  }
}

void DisplayUI::drawSpeechPet(PetMood mood, uint32_t now, uint8_t personality,
                              const PetMotionInput& motion, float zoomProgress) {
  const PetSkinDefinition& skin = petSkinDefinition(personality);
  const PetExpression expr = resolvePetExpression(mood, now, 0.5f);

  zoomProgress = constrain(zoomProgress, 0.0f, 1.0f);
  const float zoomIn = easeOutCubic(constrain(zoomProgress / 0.14f, 0.0f, 1.0f));
  const float scale = 0.62f - 0.10f * zoomIn;
  const int16_t centerX = 25;
  const int16_t centerY = 42;

  const int16_t halfGap = max<int16_t>(8, static_cast<int16_t>((skin.rightX - skin.leftX) * 0.5f * scale));
  const int16_t eyeW = max<int16_t>(6, static_cast<int16_t>(skin.eyeW * scale * 0.78f));
  const int16_t eyeH = max<int16_t>(5, static_cast<int16_t>(skin.eyeH * scale * 0.78f));
  const int16_t radius = max<int16_t>(2, static_cast<int16_t>(skin.radius * scale));

  // Keep a little of the physical orientation even in speech mode so the pet
  // still feels attached to the device, but never let motion hit the bubble.
  const int16_t shiftX = static_cast<int16_t>(roundf(constrain(motion.tiltX, -1.0f, 1.0f) * 2.0f));
  const int16_t shiftY = static_cast<int16_t>(roundf(constrain(motion.tiltY, -1.0f, 1.0f) * 1.5f));
  const int16_t leftX = centerX - halfGap + shiftX;
  const int16_t rightX = centerX + halfGap + shiftX;
  const int16_t leftY = centerY + shiftY + expr.leftY / 3;
  const int16_t rightY = centerY + shiftY + expr.rightY / 3;

  if (expr.xEyes) {
    drawXEye(leftX, leftY, max<int16_t>(8, eyeW), max<int16_t>(8, eyeH), 3);
    drawXEye(rightX, rightY, max<int16_t>(8, eyeW), max<int16_t>(8, eyeH), 3);
  } else if (expr.hearts) {
    drawHeartEye(leftX, leftY, max<uint8_t>(6, static_cast<uint8_t>(10 * scale)));
    drawHeartEye(rightX, rightY, max<uint8_t>(6, static_cast<uint8_t>(10 * scale)));
  } else {
    drawStyledEye(skin.eyeStyle, leftX, leftY, eyeW,
                  max<int16_t>(3, static_cast<int16_t>(eyeH * expr.leftOpen)),
                  radius, true, now);
    drawStyledEye(skin.eyeStyle, rightX, rightY, eyeW,
                  max<int16_t>(3, static_cast<int16_t>(eyeH * expr.rightOpen)),
                  radius, false, now);
  }

  const int16_t browY = centerY - 14 + shiftY;
  const int16_t browHalf = max<int16_t>(5, eyeW / 2);
  drawStyledBrow(skin.browStyle, leftX, browY, browHalf,
                 expr.leftBrowTilt / 2, max<int8_t>(2, expr.leftBrowArch / 2),
                 max<uint8_t>(3, skin.browThickness / 2));
  drawStyledBrow(skin.browStyle, rightX, browY, browHalf,
                 expr.rightBrowTilt / 2, max<int8_t>(2, expr.rightBrowArch / 2),
                 max<uint8_t>(3, skin.browThickness / 2));
}

void DisplayUI::drawSpeechBubble(const char* text, float progress) {
  if (!text || !text[0]) return;
  progress = constrain(progress, 0.0f, 1.0f);

  const float pop = easeOutCubic(constrain(progress / 0.10f, 0.0f, 1.0f));
  const int16_t finalX = 48;
  const int16_t finalY = 5;
  const int16_t finalW = 76;
  const int16_t finalH = 38;
  const int16_t w = max<int16_t>(8, static_cast<int16_t>(finalW * pop));
  const int16_t h = max<int16_t>(8, static_cast<int16_t>(finalH * pop));
  const int16_t x = finalX + (finalW - w) / 2;
  const int16_t y = finalY + (finalH - h) / 2;

  display_.drawRoundRect(x, y, w, h, min<int16_t>(8, min<int16_t>(w, h) / 3), OLED_WHITE);
  if (pop > 0.72f) {
    display_.drawLine(finalX + 5, finalY + finalH - 3, 39, 49, OLED_WHITE);
    display_.drawLine(finalX + 13, finalY + finalH - 1, 39, 49, OLED_WHITE);
  }
  if (progress < 0.10f) return;

  size_t fullLen = strlen(text);
  if (fullLen > 18) fullLen = 18;
  // Finish typing early, then leave a long, calm reading pause.
  const float typing = constrain((progress - 0.10f) / 0.24f, 0.0f, 1.0f);
  size_t visible = static_cast<size_t>(ceilf(fullLen * typing));
  if (visible > fullLen) visible = fullLen;

  char typed[20] = {};
  memcpy(typed, text, visible);
  typed[visible] = '\0';

  display_.setTextSize(1);
  const uint16_t tw = static_cast<uint16_t>(strlen(typed) * 6U);
  const int16_t tx = finalX + (finalW - static_cast<int16_t>(tw)) / 2;
  const int16_t ty = finalY + 15;
  display_.setCursor(max<int16_t>(finalX + 4, tx), ty);
  display_.print(typed);

  // Small typewriter cursor while letters are still appearing.
  if (visible < fullLen && ((millis() / 180UL) & 1U)) {
    display_.drawLine(min<int16_t>(finalX + finalW - 5, tx + static_cast<int16_t>(tw) + 1),
                      ty, min<int16_t>(finalX + finalW - 5, tx + static_cast<int16_t>(tw) + 1),
                      ty + 6, OLED_WHITE);
  }
}

void DisplayUI::drawSleepZzz(uint32_t now) {
  const uint8_t phase = static_cast<uint8_t>((now / 260UL) % 12UL);
  display_.setTextSize(1);
  const int16_t lift = static_cast<int16_t>(phase / 4);
  display_.setCursor(91, 30 - lift);
  display_.print("z");
  display_.setCursor(101, 20 - lift);
  display_.print("Z");
  display_.setTextSize(2);
  display_.setCursor(111, 5 - lift);
  display_.print("Z");
}

void DisplayUI::renderPet(PetMood mood, uint32_t now, float effectProgress, uint8_t personality, uint8_t accessory,
                          float pressAmount, const PetMotionInput& motion,
                          const char* speechText, float speechProgress, bool sleeping) {
  timerInitialized_ = false;
  display_.clearDisplay();

  if (speechText && speechText[0]) {
    drawSpeechPet(mood, now, personality, motion, speechProgress);
    drawSpeechBubble(speechText, speechProgress);
  } else {
    drawAccessory(accessory, now);
    drawFace(mood, now, effectProgress, personality, pressAmount, motion);
    if (sleeping) drawSleepZzz(now);
  }
  display_.display();
}

void DisplayUI::renderPomodoroReady(uint8_t timerMode, uint8_t previousTimerMode,
                                     uint16_t minutes, uint16_t previousMinutes,
                                     float modeTransition, int8_t modeDirection,
                                     float temperatureC, float pressureHpa, bool ambientAvailable, uint32_t now) {
  display_.clearDisplay();
  display_.setTextSize(1);
  timerInitialized_ = false; // Running timer starts clean after the READY screen.

  auto modeLabel = [](uint8_t mode) -> const char* {
    return mode == 0 ? "FOCUS" : (mode == 1 ? "SHORT" : "LONG");
  };
  auto formatMinutes = [](uint16_t mins, char* out, size_t outLen) {
    snprintf(out, outLen, "%02u:00", mins);
  };

  if (ambientAvailable && isfinite(temperatureC) && isfinite(pressureHpa)) {
    char ambient[20];
    snprintf(ambient, sizeof(ambient), "%.0fC %.0fhPa", temperatureC, pressureHpa);
    const int16_t x = 127 - static_cast<int16_t>(strlen(ambient) * 6);
    display_.setCursor(x > 40 ? x : 40, 3);
    display_.print(ambient);
  }

  modeTransition = constrain(modeTransition, 0.0f, 1.0f);
  const float e = easeOutCubic(modeTransition);
  const int16_t slide = 34;
  const int8_t dir = modeDirection >= 0 ? 1 : -1;

  char oldTime[8];
  char newTime[8];
  formatMinutes(previousMinutes, oldTime, sizeof(oldTime));
  formatMinutes(minutes, newTime, sizeof(newTime));

  if (modeTransition < 1.0f && previousTimerMode != timerMode) {
    const int16_t oldOffset = -dir * static_cast<int16_t>(slide * e);
    const int16_t newOffset = dir * static_cast<int16_t>(slide * (1.0f - e));
    const uint8_t oldAlpha = static_cast<uint8_t>((1.0f - e) * 16.0f);
    const uint8_t newAlpha = static_cast<uint8_t>(e * 16.0f);

    drawSmallTextLayer(modeLabel(previousTimerMode), 5 + oldOffset, 3, oldAlpha);
    drawSmallTextLayer(modeLabel(timerMode), 5 + newOffset, 3, newAlpha);
    drawTimerLayer(oldTime, 20, oldAlpha, oldOffset);
    drawTimerLayer(newTime, 20, newAlpha, newOffset);
  } else {
    drawSmallTextLayer(modeLabel(timerMode), 5, 3, 16);
    drawTimerLayer(newTime, 20, 16, 0);
  }

  display_.setCursor(5, 45);
  display_.print("<");
  centerText("PRESS TO START", 45, 1);
  display_.setCursor(117, 45);
  display_.print(">");

  // Button pulse belongs at the top, close to the physical button above the OLED.
  const uint8_t pulse = static_cast<uint8_t>((now / 90UL) % 20UL);
  const uint8_t width = pulse < 10 ? (24 + pulse * 3) : (24 + (19 - pulse) * 3);
  const int16_t x = (128 - width) / 2;
  display_.drawRoundRect(x, 12, width, 3, 1, OLED_WHITE);
  display_.display();
}


void DisplayUI::drawFocusCompanion(float progress, bool paused, uint8_t timerMode, uint32_t remainingMs, uint32_t now) {
  progress = constrain(progress, 0.0f, 1.0f);
  int16_t leftX = 56;
  int16_t rightX = 72;
  int16_t y = 50;
  int16_t w = 8;
  int16_t h = 5;

  if (paused) {
    h = 2;
  } else if (timerMode == 0) {
    // Focus becomes a little more tired over time, then perks up for the last minute.
    const bool finalMinute = remainingMs <= 60000UL;
    h = finalMinute ? 7 : max<int16_t>(3, 6 - static_cast<int16_t>(progress * 3.0f));
    if (!finalMinute && ((now / 3100UL) % 7UL) == 0UL && (now % 3100UL) < 120UL) h = 1;
  } else {
    // Breaks look relaxed and breathe slowly.
    h = 3 + (((now / 650UL) & 1UL) ? 1 : 0);
  }

  display_.fillRoundRect(leftX - w / 2, y - h / 2, w, h, min<int16_t>(3, h / 2), OLED_WHITE);
  display_.fillRoundRect(rightX - w / 2, y - h / 2, w, h, min<int16_t>(3, h / 2), OLED_WHITE);
}

void DisplayUI::renderPomodoro(uint32_t remainingMs, uint32_t totalMs, bool paused, uint8_t timerMode, float temperatureC, float pressureHpa, bool ambientAvailable, bool focusCompanion, uint32_t now) {
  display_.clearDisplay();
  display_.setTextSize(1);
  const bool isBreak = timerMode != 0;
  const char* label = timerMode == 0 ? "FOCUS" : (timerMode == 1 ? "SHORT" : "LONG");
  display_.setCursor(5, 3);
  display_.print(label);
  if (ambientAvailable && isfinite(temperatureC) && isfinite(pressureHpa)) {
    char ambient[20];
    snprintf(ambient, sizeof(ambient), "%.0fC %.0fhPa", temperatureC, pressureHpa);
    const int16_t x = 127 - static_cast<int16_t>(strlen(ambient) * 6);
    display_.setCursor(x > 40 ? x : 40, 3);
    display_.print(ambient);
  }

  const uint32_t totalSeconds = (remainingMs + 999) / 1000;
  if (!timerInitialized_) {
    timerInitialized_ = true;
    timerShownSeconds_ = totalSeconds;
    timerFromSeconds_ = totalSeconds;
    timerToSeconds_ = totalSeconds;
    timerTransitionAt_ = now;
  } else if (totalSeconds != timerToSeconds_) {
    timerFromSeconds_ = timerToSeconds_;
    timerToSeconds_ = totalSeconds;
    timerShownSeconds_ = totalSeconds;
    timerTransitionAt_ = now;
  }

  auto formatTime = [](uint32_t secondsValue, char* out, size_t outLen) {
    const uint16_t minutesValue = secondsValue / 60;
    const uint8_t seconds = secondsValue % 60;
    snprintf(out, outLen, "%02u:%02u", minutesValue, seconds);
  };

  char fromText[8];
  char toText[8];
  formatTime(timerFromSeconds_, fromText, sizeof(fromText));
  formatTime(timerToSeconds_, toText, sizeof(toText));

  constexpr uint16_t transitionMs = 300;
  float t = static_cast<float>(now - timerTransitionAt_) / static_cast<float>(transitionMs);
  if (t > 1.0f) t = 1.0f;
  const float e = easeOutCubic(t);
  const int16_t glyphW = 12;
  const int16_t startX = (hw::SCREEN_WIDTH - 5 * glyphW) / 2;
  const int16_t baseY = 20;
  const int16_t travel = 12;

  for (uint8_t i = 0; i < 5; ++i) {
    const int16_t x = startX + static_cast<int16_t>(i) * glyphW;
    const bool changing = t < 1.0f && fromText[i] != toText[i];
    if (!changing) {
      drawTimerGlyph(toText[i], x, baseY, 16);
      continue;
    }

    const int16_t oldY = baseY - static_cast<int16_t>(travel * e);
    const int16_t newY = baseY + static_cast<int16_t>(travel * (1.0f - e));
    const uint8_t oldAlpha = static_cast<uint8_t>((1.0f - e) * 16.0f);
    const uint8_t newAlpha = static_cast<uint8_t>(e * 16.0f);
    drawTimerGlyph(fromText[i], x, oldY, oldAlpha);
    drawTimerGlyph(toText[i], x, newY, newAlpha);
  }

  if (focusCompanion) {
    const float progress = totalMs ? constrain(static_cast<float>(totalMs - remainingMs) / static_cast<float>(totalMs), 0.0f, 1.0f) : 1.0f;
    drawFocusCompanion(progress, paused, timerMode, remainingMs, now);
  } else {
    if (paused) centerText("PAUSED", 43, 1);
    else centerText(isBreak ? "RECOVER" : "DEEP WORK", 43, 1);
  }

  display_.drawRoundRect(7, 55, 114, 7, 3, OLED_WHITE);
  const uint16_t fill = totalMs ? static_cast<uint16_t>((110ULL * (totalMs - remainingMs)) / totalMs) : 110;
  if (fill > 0) display_.fillRoundRect(9, 57, (fill < 110) ? fill : 110, 3, 1, OLED_WHITE);
  display_.display();
}

void DisplayUI::renderStats(const PetState& state) {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(4, 3);
  display_.print("LV ");
  display_.print(state.level);
  display_.setCursor(79, 3);
  display_.print("XP ");
  display_.print(state.xp);

  display_.setCursor(6, 20);
  display_.print("MOOD");
  display_.setCursor(92, 20);
  display_.print(state.mood);
  display_.drawRoundRect(37, 20, 49, 7, 3, OLED_WHITE);
  display_.fillRoundRect(39, 22, static_cast<uint8_t>(45UL * state.mood / 100UL), 3, 1, OLED_WHITE);

  display_.setCursor(6, 35);
  display_.print("ENER");
  display_.setCursor(92, 35);
  display_.print(state.energy);
  display_.drawRoundRect(37, 35, 49, 7, 3, OLED_WHITE);
  display_.fillRoundRect(39, 37, static_cast<uint8_t>(45UL * state.energy / 100UL), 3, 1, OLED_WHITE);

  display_.setCursor(6, 50);
  display_.print("BOND");
  display_.setCursor(92, 50);
  display_.print(state.affection);
  display_.drawRoundRect(37, 50, 49, 7, 3, OLED_WHITE);
  display_.fillRoundRect(39, 52, static_cast<uint8_t>(45UL * state.affection / 100UL), 3, 1, OLED_WHITE);
  display_.display();
}

void DisplayUI::renderConfigHint() {
  display_.clearDisplay();
  centerText("CONFIG", 8, 1);
  centerText("BLE READY", 25, 2);
  centerText("XIAO Computer Pet", 52, 1);
  display_.display();
}

float DisplayUI::easeOutCubic(float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  const float u = 1.0f - t;
  return 1.0f - (u * u * u);
}

void DisplayUI::drawMenuHeader(const char* label) {
  display_.setTextSize(1);
  display_.setCursor(5, 3);
  display_.print("PET");
  display_.drawLine(25, 7, 34, 7, OLED_WHITE);
  display_.setCursor(39, 3);
  display_.print(label);
  display_.drawLine(4, 13, 123, 13, OLED_WHITE);
}

void DisplayUI::drawMenuFooter(uint8_t index, uint8_t count) {
  if (count == 0) return;
  const int16_t spacing = 7;
  const int16_t total = static_cast<int16_t>((count - 1) * spacing + 3);
  int16_t x = (hw::SCREEN_WIDTH - total) / 2;
  for (uint8_t i = 0; i < count; ++i) {
    if (i == index) display_.fillRoundRect(x - 1, 59, 5, 3, 1, OLED_WHITE);
    else display_.drawPixel(x + 1, 60, OLED_WHITE);
    x += spacing;
  }
}

void DisplayUI::drawMenuIcon(uint8_t icon, int16_t cx, int16_t cy, uint32_t now) {
  switch (icon) {
    case 0: // Stats
      display_.drawRoundRect(cx - 10, cy - 9, 20, 18, 4, OLED_WHITE);
      display_.fillRect(cx - 6, cy + 1, 3, 5, OLED_WHITE);
      display_.fillRect(cx - 1, cy - 3, 3, 9, OLED_WHITE);
      display_.fillRect(cx + 4, cy - 6, 3, 12, OLED_WHITE);
      break;
    case 1: // Accessory
      drawSparkle(cx, cy, 7);
      drawSparkle(cx + 9, cy - 7, 2);
      break;
    case 2: // Style
      display_.fillRoundRect(cx - 10, cy - 6, 7, 12, 3, OLED_WHITE);
      display_.fillRoundRect(cx + 3, cy - 6, 7, 12, 3, OLED_WHITE);
      display_.drawLine(cx - 10, cy - 11, cx - 3, cy - 13, OLED_WHITE);
      display_.drawLine(cx + 3, cy - 13, cx + 10, cy - 11, OLED_WHITE);
      break;
    case 3: // Sound
      display_.fillRect(cx - 10, cy - 3, 5, 7, OLED_WHITE);
      display_.drawLine(cx - 5, cy - 3, cx, cy - 8, OLED_WHITE);
      display_.drawLine(cx, cy - 8, cx, cy + 8, OLED_WHITE);
      display_.drawLine(cx, cy + 8, cx - 5, cy + 4, OLED_WHITE);
      display_.drawCircle(cx + 3, cy, 6, OLED_WHITE);
      display_.drawCircle(cx + 3, cy, 9, OLED_WHITE);
      break;
    case 4: // Sleep
      display_.fillCircle(cx - 1, cy, 9, OLED_WHITE);
      display_.fillCircle(cx + 4, cy - 4, 9, 0);
      break;
    case 5: // Exit
      display_.drawRoundRect(cx - 9, cy - 10, 13, 20, 3, OLED_WHITE);
      display_.drawLine(cx, cy, cx + 11, cy, OLED_WHITE);
      display_.drawLine(cx + 7, cy - 4, cx + 11, cy, OLED_WHITE);
      display_.drawLine(cx + 7, cy + 4, cx + 11, cy, OLED_WHITE);
      break;
    case 6: { // Spark accessory
      const uint8_t s = ((now / 180) & 1) ? 7 : 5;
      drawSparkle(cx, cy, s);
      break;
    }
    case 7: // Orbit accessory
      display_.drawRoundRect(cx - 13, cy - 5, 26, 10, 5, OLED_WHITE);
      display_.fillCircle(cx + static_cast<int8_t>((now / 180) % 3) - 1, cy - 6, 2, OLED_WHITE);
      break;
    case 8: // Crown
      display_.drawLine(cx - 11, cy + 6, cx - 8, cy - 6, OLED_WHITE);
      display_.drawLine(cx - 8, cy - 6, cx - 2, cy + 1, OLED_WHITE);
      display_.drawLine(cx - 2, cy + 1, cx + 3, cy - 7, OLED_WHITE);
      display_.drawLine(cx + 3, cy - 7, cx + 10, cy + 5, OLED_WHITE);
      display_.drawLine(cx - 11, cy + 6, cx + 10, cy + 6, OLED_WHITE);
      break;
    case 9: // Toggle
      display_.drawRoundRect(cx - 12, cy - 6, 24, 12, 6, OLED_WHITE);
      display_.fillCircle(cx + 6, cy, 4, OLED_WHITE);
      break;
    case 11: // Rotate screen
      display_.drawCircle(cx, cy, 10, OLED_WHITE);
      display_.fillTriangle(cx + 7, cy - 9, cx + 13, cy - 7, cx + 9, cy - 3, OLED_WHITE);
      display_.fillTriangle(cx - 7, cy + 9, cx - 13, cy + 7, cx - 9, cy + 3, OLED_WHITE);
      display_.drawLine(cx - 8, cy - 6, cx - 3, cy - 10, OLED_WHITE);
      display_.drawLine(cx + 8, cy + 6, cx + 3, cy + 10, OLED_WHITE);
      break;
    case 12: // Swap left/right touch
      display_.drawLine(cx - 12, cy, cx + 12, cy, OLED_WHITE);
      display_.fillTriangle(cx - 12, cy, cx - 6, cy - 5, cx - 6, cy + 5, OLED_WHITE);
      display_.fillTriangle(cx + 12, cy, cx + 6, cy - 5, cx + 6, cy + 5, OLED_WHITE);
      break;
    case 13: // MPU calibration / level target
      display_.drawCircle(cx, cy, 10, OLED_WHITE);
      display_.drawLine(cx - 14, cy, cx - 6, cy, OLED_WHITE);
      display_.drawLine(cx + 6, cy, cx + 14, cy, OLED_WHITE);
      display_.drawLine(cx, cy - 14, cx, cy - 6, OLED_WHITE);
      display_.drawLine(cx, cy + 6, cx, cy + 14, OLED_WHITE);
      display_.fillCircle(cx, cy, 2, OLED_WHITE);
      break;
    case 10: // Save & back
      display_.drawRoundRect(cx - 11, cy - 9, 17, 18, 3, OLED_WHITE);
      display_.drawLine(cx - 7, cy + 1, cx - 3, cy + 5, OLED_WHITE);
      display_.drawLine(cx - 3, cy + 5, cx + 3, cy - 3, OLED_WHITE);
      display_.drawLine(cx + 5, cy, cx + 12, cy, OLED_WHITE);
      display_.drawLine(cx + 8, cy - 4, cx + 12, cy, OLED_WHITE);
      display_.drawLine(cx + 8, cy + 4, cx + 12, cy, OLED_WHITE);
      break;
    default:
      display_.drawCircle(cx, cy, 8, OLED_WHITE);
      break;
  }
}

void DisplayUI::drawMenuCard(int16_t x, const char* title, const char* detail, uint8_t icon, uint32_t now, bool active) {
  const int16_t y = 16;
  const int16_t w = 104;
  const int16_t h = 40;
  display_.drawRoundRect(x, y, w, h, 8, OLED_WHITE);

  // Neighbour cards are deliberately text-free: only the central card owns copy.
  // This avoids clipped labels while preserving the carousel preview at both edges.
  drawMenuIcon(icon, active ? x + 19 : x + w / 2, y + 20, now);
  if (!active) return;

  display_.setTextSize(1);
  display_.setCursor(x + 36, y + 7);
  display_.print(title);
  display_.setCursor(x + 36, y + 23);
  display_.print(detail);
  display_.fillCircle(x + w - 7, y + 7, 1, OLED_WHITE);
}

void DisplayUI::drawStatsCard(int16_t x, uint8_t page, const PetState& state, uint32_t now, bool active) {
  (void)now;
  const int16_t y = 16;
  const int16_t w = 104;
  const int16_t h = 40;
  display_.drawRoundRect(x, y, w, h, 8, OLED_WHITE);
  if (!active) return;
  display_.fillCircle(x + w - 7, y + 7, 1, OLED_WHITE);
  display_.setTextSize(1);

  if (page == 0) {
    display_.setCursor(x + 7, y + 3); display_.print("VITALS");
    const uint8_t vals[3] = { state.mood, state.energy, state.affection };
    const char* labs[3] = { "M", "E", "A" };
    for (uint8_t i = 0; i < 3; ++i) {
      const int16_t yy = y + 14 + i * 9;
      display_.setCursor(x + 7, yy); display_.print(labs[i]);
      display_.drawRoundRect(x + 17, yy + 1, 55, 4, 2, OLED_WHITE);
      const uint8_t fill = static_cast<uint8_t>(51UL * vals[i] / 100UL);
      if (fill) display_.fillRect(x + 19, yy + 2, fill, 2, OLED_WHITE);
      display_.setCursor(x + 78, yy); display_.print(vals[i]);
    }
  } else if (page == 1) {
    display_.setCursor(x + 7, y + 4); display_.print("FOCUS");
    display_.setCursor(x + 7, y + 17); display_.print("TODAY "); display_.print(state.todaySessions); display_.print(" / "); display_.print(state.todayMinutes); display_.print("m");
    display_.setCursor(x + 7, y + 29); display_.print("ALL   "); display_.print(state.focusSessions); display_.print(" / "); display_.print(state.focusMinutes); display_.print("m");
  } else {
    display_.setCursor(x + 7, y + 4); display_.print("PROGRESS");
    display_.setCursor(x + 7, y + 17); display_.print("LV "); display_.print(state.level); display_.print("  XP "); display_.print(state.xp);
    display_.setCursor(x + 7, y + 29); display_.print("STREAK "); display_.print(state.streakDays); display_.print("d");
  }
}

void DisplayUI::drawHoldProgress(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, float progress) {
  if (progress <= 0.0f) return;
  if (progress > 1.0f) progress = 1.0f;

  // Draw a second, thicker rounded trace clockwise from the top centre.
  x += 2;
  y += 2;
  w -= 4;
  h -= 4;
  const int16_t maxRadius = ((w < h) ? w : h) / 2;
  if (radius > maxRadius) radius = maxRadius;

  const float arc = 1.5707963f * radius;
  const float topHalf = (static_cast<float>(w) / 2.0f) - radius;
  const float side = h - 2.0f * radius;
  const float bottom = w - 2.0f * radius;
  const float total = topHalf + arc + side + arc + bottom + arc + side + arc + topHalf;
  const float target = total * progress;

  auto plot = [&](float px, float py) {
    display_.fillCircle(static_cast<int16_t>(px + 0.5f), static_cast<int16_t>(py + 0.5f), 1, OLED_WHITE);
  };

  for (float d = 0.0f; d <= target; d += 1.15f) {
    float q = d;
    float px = 0.0f, py = 0.0f;

    if (q <= topHalf) {
      px = x + w / 2.0f + q; py = y;
    } else if ((q -= topHalf) <= arc) {
      const float a = -1.5707963f + q / radius;
      px = x + w - radius + cosf(a) * radius;
      py = y + radius + sinf(a) * radius;
    } else if ((q -= arc) <= side) {
      px = x + w; py = y + radius + q;
    } else if ((q -= side) <= arc) {
      const float a = q / radius;
      px = x + w - radius + cosf(a) * radius;
      py = y + h - radius + sinf(a) * radius;
    } else if ((q -= arc) <= bottom) {
      px = x + w - radius - q; py = y + h;
    } else if ((q -= bottom) <= arc) {
      const float a = 1.5707963f + q / radius;
      px = x + radius + cosf(a) * radius;
      py = y + h - radius + sinf(a) * radius;
    } else if ((q -= arc) <= side) {
      px = x; py = y + h - radius - q;
    } else if ((q -= side) <= arc) {
      const float a = 3.1415926f + q / radius;
      px = x + radius + cosf(a) * radius;
      py = y + radius + sinf(a) * radius;
    } else {
      q -= arc;
      px = x + radius + q; py = y;
    }
    plot(px, py);
  }
}

void DisplayUI::renderPetMenu(PetMenuView view, uint8_t index, uint8_t previousIndex, float transition,
                              int8_t direction, uint32_t now, const PetState& state,
                              uint8_t highestUnlockedAccessory, float holdProgress) {
  display_.clearDisplay();

  const char* header = "MENU";
  uint8_t count = 1;
  if (view == PetMenuView::Root) { header = "MENU"; count = 5; }
  else if (view == PetMenuView::Stats) { header = "STATS"; count = 4; }
  else if (view == PetMenuView::Accessory) { header = "GEAR"; count = 6; }
  else if (view == PetMenuView::Personality) { header = "STYLE"; count = PET_SKIN_COUNT + 1; }
  else if (view == PetMenuView::Options) { header = "OPTIONS"; count = 10; }
  else if (view == PetMenuView::DeskBuddy) { header = "DESK"; count = 3; }
  else if (view == PetMenuView::Sound) { header = "SOUND"; count = 3; }
  else if (view == PetMenuView::Sleep) { header = "SLEEP"; count = 3; }
  else if (view == PetMenuView::PetSound) { header = "PET SFX"; count = 4; }
  else if (view == PetMenuView::DoneMelody) { header = "FOCUS FX"; count = 5; }
  else if (view == PetMenuView::BreakMelody) { header = "BREAK FX"; count = 4; }
  else if (view == PetMenuView::ScreenRotation) { header = "SCREEN"; count = 3; }
  else if (view == PetMenuView::TouchLayout) { header = "TOUCH"; count = 3; }
  else if (view == PetMenuView::MpuCalibration) { header = "MPU CAL"; count = 3; }
  drawMenuHeader(header);

  const float eased = easeOutCubic(transition);
  constexpr int16_t cardX = 12;
  constexpr int16_t cardY = 16;
  constexpr int16_t cardW = 104;
  constexpr int16_t cardH = 40;
  constexpr int16_t cardRadius = 8;
  constexpr int16_t spacing = 112; // Leaves a 4 px peek of neighbours at each edge.

  auto rootCard = [&](uint8_t item, int16_t x, bool active) {
    static const char* titles[] = { "STATUS", "ACCESSORY", "STYLE", "OPTIONS", "EXIT" };
    static const char* details[] = { "pet stats", "gear", "eye shape", "settings", "go home" };
    static const uint8_t icons[] = { 0, 1, 2, 3, 5 };
    drawMenuCard(x, titles[item % 5], details[item % 5], icons[item % 5], now, active);
  };

  auto accessoryCard = [&](uint8_t item, int16_t x, bool active) {
    const char* title = "AUTO";
    const char* detail = "best gear";
    uint8_t icon = 1;
    bool locked = false;
    if (item == 1) { title = "NONE"; detail = "clean face"; icon = 2; }
    if (item == 2) { title = "SPARK"; detail = "level 2"; icon = 6; locked = highestUnlockedAccessory < 1; }
    if (item == 3) { title = "ORBIT"; detail = "level 4"; icon = 7; locked = highestUnlockedAccessory < 2; }
    if (item == 4) { title = "CROWN"; detail = "level 7"; icon = 8; locked = highestUnlockedAccessory < 3; }
    if (locked) detail = "LOCKED";
    drawMenuCard(x, title, detail, icon, now, active);
  };

  auto styleCard = [&](uint8_t item, int16_t x, bool active) {
    drawMenuCard(x, petSkinName(item), petSkinDetail(item), 2, now, active);
  };

  auto toggleCard = [&](bool sleep, uint8_t item, int16_t x, bool active) {
    const bool enabled = item == 1;
    drawMenuCard(x, enabled ? "ENABLED" : "DISABLED", sleep ? "sleep time" : "buzzer", 9, now, active);
  };

  auto optionsCard = [&](uint8_t item, int16_t x, bool active) {
    static const char* titles[] = { "DESK BUDDY", "SOUND", "SLEEP", "SCREEN", "TOUCH SIDE", "MPU CAL", "PET SFX", "FOCUS FX", "BREAK FX" };
    static const char* details[] = { "sensors", "buzzer", "schedule", "rotation", "left / right", "set neutral", "pet sound", "focus tune", "break tune" };
    static const uint8_t icons[] = { 2, 3, 4, 11, 12, 13, 3, 3, 3 };
    drawMenuCard(x, titles[item % 9], details[item % 9], icons[item % 9], now, active);
  };

  auto petSoundCard = [&](uint8_t item, int16_t x, bool active) {
    static const char* titles[] = { "CHIRP", "PING", "STEPS" };
    drawMenuCard(x, titles[item % 3], "pet sound", 3, now, active);
  };

  auto doneMelodyCard = [&](uint8_t item, int16_t x, bool active) {
    static const char* titles[] = { "VICTORY", "ASCEND", "CLEAR", "SIGNAL" };
    drawMenuCard(x, titles[item % 4], "focus done", 3, now, active);
  };

  auto breakMelodyCard = [&](uint8_t item, int16_t x, bool active) {
    static const char* titles[] = { "SOFT DOWN", "LANDING", "DOUBLE" };
    drawMenuCard(x, titles[item % 3], "break done", 3, now, active);
  };

  auto drawCard = [&](uint8_t item, int16_t x, bool active) {
    if (view != PetMenuView::Root && item == count - 1) {
      drawMenuCard(x, "SAVE & BACK", "save + back", 10, now, active);
      return;
    }

    if (view == PetMenuView::Root) rootCard(item, x, active);
    else if (view == PetMenuView::Stats) drawStatsCard(x, item, state, now, active);
    else if (view == PetMenuView::Accessory) accessoryCard(item, x, active);
    else if (view == PetMenuView::Personality) styleCard(item, x, active);
    else if (view == PetMenuView::Options) optionsCard(item, x, active);
    else if (view == PetMenuView::DeskBuddy) drawMenuCard(x, item == 1 ? "ENABLED" : "DISABLED", "desk mode", 2, now, active);
    else if (view == PetMenuView::Sound) toggleCard(false, item, x, active);
    else if (view == PetMenuView::Sleep) toggleCard(true, item, x, active);
    else if (view == PetMenuView::PetSound) petSoundCard(item, x, active);
    else if (view == PetMenuView::DoneMelody) doneMelodyCard(item, x, active);
    else if (view == PetMenuView::BreakMelody) breakMelodyCard(item, x, active);
    else if (view == PetMenuView::ScreenRotation) drawMenuCard(x, item == 1 ? "ROTATED" : "NORMAL", item == 1 ? "180 degrees" : "0 degrees", 11, now, active);
    else if (view == PetMenuView::TouchLayout) drawMenuCard(x, item == 1 ? "SWAPPED" : "NORMAL", item == 1 ? "D3 right" : "D3 left", 12, now, active);
    else if (view == PetMenuView::MpuCalibration) drawMenuCard(x, item == 0 ? "CALIBRATE" : "RESET CAL", item == 0 ? "keep still" : "clear zero", 13, now, active);
  };

  auto wrapped = [&](int16_t value) -> uint8_t {
    while (value < 0) value += count;
    while (value >= count) value -= count;
    return static_cast<uint8_t>(value);
  };

  int16_t activeX = cardX;
  if (transition < 1.0f && previousIndex != index) {
    activeX = cardX + static_cast<int16_t>((1.0f - eased) * spacing * direction);
    const int16_t oldX = cardX - static_cast<int16_t>(eased * spacing * direction);
    const uint8_t outer = wrapped(static_cast<int16_t>(index) + direction);
    drawCard(outer, activeX + spacing * direction, false);
    drawCard(previousIndex, oldX, false);
    drawCard(index, activeX, true);
  } else {
    drawCard(wrapped(static_cast<int16_t>(index) - 1), cardX - spacing, false);
    drawCard(wrapped(static_cast<int16_t>(index) + 1), cardX + spacing, false);
    drawCard(index, cardX, true);
    activeX = cardX;
  }

  if (holdProgress > 0.0f) {
    drawHoldProgress(activeX, cardY, cardW, cardH, cardRadius, holdProgress);
  }

  drawMenuFooter(index, count);
  display_.display();
}

