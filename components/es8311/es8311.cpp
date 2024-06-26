#include "es8311.h"
#include "es8311_const.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace es8311 {

static const char *const TAG = "es8311";

#define ES8311_ERROR_CHECK(func) \
  if (!(func)) { \
    this->mark_failed(); \
    ESP_LOGE(TAG, "Operation failed at: " #func); \
    return; \
  }
#define ES8311_READ_BYTE(reg, value) ES8311_ERROR_CHECK(this->read_byte(reg, value));
#define ES8311_WRITE_BYTE(reg, value) ES8311_ERROR_CHECK(this->write_byte(reg, value));

void ES8311Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ES8311...");

  // Reset
  ESP_LOGCONFIG(TAG, "Resetting ES8311...");
  ES8311_WRITE_BYTE(ES8311_REG00_RESET, 0x1F);
  delay(20);
  ES8311_WRITE_BYTE(ES8311_REG00_RESET, 0x00);

  // Configure clock and audio format
  this->configure_clock_();
  this->configure_format_();

  // Check if any operation has failed during configuration
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Failed to initialize!");
    return;
  }

  // Set initial volume
  this->set_volume(0.75);  // 0.75 = 0xBF = 0dB

  // Configure microphone
  this->configure_microphone_();

  // Power up
  ESP_LOGCONFIG(TAG, "Powering up ES8311...");
  ES8311_WRITE_BYTE(ES8311_REG0D_SYSTEM, 0x01);  // Power up analog circuitry
  ES8311_WRITE_BYTE(ES8311_REG0E_SYSTEM, 0x02);  // Enable analog PGA, enable ADC modulator
  ES8311_WRITE_BYTE(ES8311_REG12_SYSTEM, 0x00);  // Power up DAC
  ES8311_WRITE_BYTE(ES8311_REG13_SYSTEM, 0x10);  // Enable output to HP drive
  ES8311_WRITE_BYTE(ES8311_REG1C_ADC, 0x6A);     // ADC Equalizer bypass, cancel DC offset in digital domain
  ES8311_WRITE_BYTE(ES8311_REG37_DAC, 0x08);     // Bypass DAC equalizer
  ES8311_WRITE_BYTE(ES8311_REG00_RESET, 0x80);   // Power On

  ESP_LOGCONFIG(TAG, "ES8311 setup complete.");
}

void ES8311Component::configure_clock_() {
  ESP_LOGCONFIG(TAG, "Configuring ES8311 clock...");
  
  // Register 0x01: select clock source for internal MCLK and determine its frequency
  uint8_t reg01 = 0x3F;  // Enable all clocks, set for slave mode
  reg01 |= BIT(7);       // Use SCLK instead of MCLK
  if (this->mclk_inverted_) {
    reg01 |= BIT(6);  // Invert MCLK pin
  }
  ES8311_WRITE_BYTE(ES8311_REG01_CLK_MANAGER, reg01);

  // Set clock dividers and multipliers directly for 16kHz sample rate and 16-bit resolution
  const ES8311Coefficient *coeff = get_coefficient(4096000, 16000);
  if (coeff != nullptr) {
    ES8311_WRITE_BYTE(ES8311_REG02_CLK_MANAGER, coeff->pre_div);
    ES8311_WRITE_BYTE(ES8311_REG03_CLK_MANAGER, coeff->fs_mode);
    ES8311_WRITE_BYTE(ES8311_REG04_CLK_MANAGER, coeff->dac_osr);
    ES8311_WRITE_BYTE(ES8311_REG05_CLK_MANAGER, (coeff->adc_div << 4) | coeff->dac_div);
    ES8311_WRITE_BYTE(ES8311_REG06_CLK_MANAGER, (this->sclk_inverted_ ? BIT(5) : 0) | coeff->bclk_div);
    ES8311_WRITE_BYTE(ES8311_REG07_CLK_MANAGER, coeff->lrck_h);
    ES8311_WRITE_BYTE(ES8311_REG08_CLK_MANAGER, coeff->lrck_l);
  }

  ESP_LOGCONFIG(TAG, "ES8311 clock configured.");
}

void ES8311Component::configure_format_() {
  ESP_LOGCONFIG(TAG, "Configuring ES8311 format...");
  // Configure I2S mode and format
  uint8_t reg09;
  ES8311_READ_BYTE(ES8311_REG09_SDPIN, &reg09);
  reg09 &= 0x80; // Clear existing format bits
  reg09 |= (1 << 4); // Set I2S format (Standard I2S)
  ES8311_WRITE_BYTE(ES8311_REG09_SDPIN, reg09);

  // Configure sample rate to 16kHz (ADC)
  uint8_t reg0A;
  ES8311_READ_BYTE(ES8311_REG0A_SDPOUT, &reg0A);
  reg0A &= 0xF0; // Clear existing sample rate bits
  reg0A |= 0x02; // Set sample rate to 16kHz (Standard setting for 16kHz)
  ES8311_WRITE_BYTE(ES8311_REG0A_SDPOUT, reg0A);

  ESP_LOGCONFIG(TAG, "ES8311 format configured.");
}

uint8_t ES8311Component::calculate_resolution_value(ES8311Resolution resolution) {
  switch (resolution) {
    case ES8311_RESOLUTION_16:
      return (3 << 2);
    case ES8311_RESOLUTION_18:
      return (2 << 2);
    case ES8311_RESOLUTION_20:
      return (1 << 2);
    case ES8311_RESOLUTION_24:
      return (0 << 2);
    case ES8311_RESOLUTION_32:
      return (4 << 2);
    default:
      return 0;
  }
}

void ES8311Component::configure_microphone_() {
  uint8_t reg14 = 0x1A;  // Enable analog MIC and max PGA gain
  if (this->use_microphone_) {
    reg14 |= BIT(6);  // Enable PDM digital microphone
  }
  ES8311_WRITE_BYTE(ES8311_REG14_SYSTEM, reg14);

  ES8311_WRITE_BYTE(ES8311_REG16_ADC, this->microphone_gain_);  // ADC gain scale up
  ES8311_WRITE_BYTE(ES8311_REG17_ADC, 0xC8);                    // Set ADC gain
}

void ES8311Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8311 Audio Codec:");
  ESP_LOGCONFIG(TAG, "  Use MCLK: %s", YESNO(this->use_mclk_));
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Failed to initialize!");
    return;
  }
#ifdef ESPHOME_LOG_HAS_VERBOSE
  ESP_LOGV(TAG, "  Register Values:");
  for (uint8_t reg = 0; reg <= 0x45; reg++) {
    uint8_t value;
    ES8311_READ_BYTE(reg, &value);
    ESP_LOGV(TAG, "    %02x = %02x", reg, value);
  }
#endif
}

void ES8311Component::set_volume(float volume) {
  volume = clamp(volume, 0.0f, 1.0f);
  uint8_t reg32 = remap<uint8_t, float>(volume, 0.0f, 1.0f, 0, 255);
  ES8311_WRITE_BYTE(ES8311_REG32_DAC, reg32);
}

float ES8311Component::get_volume() {
  uint8_t reg32;
  this->read_byte(ES8311_REG32_DAC, &reg32);
  return remap<uint8_t, float>(reg32, 0, 255, 0.0f, 1.0f);
}

void ES8311Component::set_mute(bool mute) {
  uint8_t reg31;
  ES8311_READ_BYTE(ES8311_REG31_DAC, &reg31);

  if (mute) {
    reg31 |= BIT(6) | BIT(5);
  } else {
    reg31 &= ~(BIT(6) | BIT(5));
  }

  ES8311_WRITE_BYTE(ES8311_REG31_DAC, reg31);
}

const ES8311Coefficient *ES8311Component::get_coefficient(uint32_t mclk, uint32_t rate) {
  for (const auto &coefficient : ES8311_COEFFICIENTS) {
    if (coefficient.mclk == mclk && coefficient.rate == rate)
      return &coefficient;
  }
  return nullptr;
}

}  // namespace es8311
}  // namespace esphome
