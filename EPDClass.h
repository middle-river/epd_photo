// Library for the E-Ink ED060XC3.
// 2021-11-28  T. Nakagawa

#ifndef EPDCLASS_H_
#define EPDCLASS_H_

#include <Arduino.h>

class EPDClass {
public:
  EPDClass(int pin_spv, int pin_ckv, int pin_mode, int pin_stl, int pin_oe, int pin_le, int pin_cl,int pin_d0, int pin_dcdc) : pin_spv_(pin_spv), pin_ckv_(pin_ckv), pin_mode_(pin_mode), pin_stl_(pin_stl), pin_oe_(pin_oe), pin_le_(pin_le), pin_cl_(pin_cl), pin_d0_(pin_d0), pin_dcdc_(pin_dcdc) {
    digitalWrite(pin_spv_, HIGH);
    pinMode(pin_spv_, OUTPUT);
    digitalWrite(pin_ckv_, HIGH);
    pinMode(pin_ckv_, OUTPUT);
    pinMode(pin_mode_, OUTPUT);
    digitalWrite(pin_stl_, HIGH);
    pinMode(pin_stl_, OUTPUT);
    pinMode(pin_oe_, OUTPUT);
    pinMode(pin_le_, OUTPUT);
    pinMode(pin_cl_, OUTPUT);
    pinModeByte(pin_d0_, OUTPUT);
    digitalWrite(pin_dcdc_, HIGH);
    pinMode(pin_dcdc_, OUTPUT);
  }

  void enable() {
    digitalWrite(pin_dcdc_, LOW);
    delay(100);
  }

  void disable() {
    digitalWrite(pin_dcdc_, HIGH);
  }

  void startFrame() {
    digitalWrite(pin_oe_, HIGH);
    digitalWrite(pin_mode_, HIGH);

    digitalWrite(pin_spv_, LOW);
    digitalWrite(pin_ckv_, LOW);
    delayMicroseconds(1);
    digitalWrite(pin_ckv_, HIGH);
    digitalWrite(pin_spv_, HIGH);
  }

  void endFrame() {
    digitalWrite(pin_mode_, LOW);
    digitalWrite(pin_oe_, LOW);
  }

  void writeRow(int size, const uint8_t *data) {
    digitalWrite(pin_stl_, LOW);
    for (int i = 0; i < size; i++) {
      digitalWriteByte(pin_d0_, data[i]);
      digitalWrite(pin_cl_, HIGH);
      digitalWrite(pin_cl_, LOW);
    }
    digitalWrite(pin_stl_, HIGH);
    digitalWrite(pin_cl_, HIGH);
    digitalWrite(pin_cl_, LOW);

    digitalWrite(pin_le_, HIGH);
    digitalWrite(pin_le_, LOW);

    digitalWrite(pin_ckv_, LOW);
    delayMicroseconds(1);
    digitalWrite(pin_ckv_, HIGH);
  }

private:
  static void pinModeByte(int pin0, int mode) {
    for (int i = 0; i < 8; i++) pinMode(pin0 + i, mode);
    if (mode == INPUT) {
      REG_WRITE(GPIO_ENABLE_W1TC_REG, 0xff << pin0);
    } else if (mode == OUTPUT) {
      REG_WRITE(GPIO_ENABLE_W1TS_REG, 0xff << pin0);
    }
  }

  static uint8_t digitalReadByte(int pin0) {
    return (uint8_t)(REG_READ(GPIO_IN_REG >> pin0));
  }

  static void digitalWriteByte(int pin0, uint8_t value) {
    REG_WRITE(GPIO_OUT_REG, (REG_READ(GPIO_OUT_REG) & ~(0xff << pin0)) | (((uint32_t)value) << pin0));
  }

  int pin_spv_;
  int pin_ckv_;
  int pin_mode_;
  int pin_stl_;
  int pin_oe_;
  int pin_le_;
  int pin_cl_;
  int pin_d0_;
  int pin_dcdc_;
};

#endif
