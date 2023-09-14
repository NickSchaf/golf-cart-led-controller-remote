#ifndef EXAMPLE_BLE_SEC_GATTC_DEMO_H
#define EXAMPLE_BLE_SEC_GATTC_DEMO_H


#ifdef __cplusplus
extern "C" {
#endif


typedef void (*ValueChangedCb)(uint8_t value);
typedef void (*StrListRecdCb)(const char * str, int strlen);
typedef void (*ConnectChangedCb)(bool connected);

void init_gatt_client();

void SetPatternChangedCallback(ValueChangedCb fn);
void SetColorChangedCallback(ValueChangedCb fn);
void SetBrightnessChangedCallback(ValueChangedCb fn);
void SetSpeedChangedCallback(ValueChangedCb fn);

void SetPatternListCallback(StrListRecdCb fn);
void SetColorListCallback(StrListRecdCb fn);

void SetConnectChangedCallback(ConnectChangedCb fn);

void SetPatternIdx(uint8_t index);
void SetColorIdx(uint8_t index);
void SetBrightness(uint8_t brightness);
void SetSpeed(uint8_t speed);

void BeginReadCurPattern();
void BeginReadCurColor();
void BeginReadBrightness();
void BeginReadSpeed();
void BeginReadPatterns();
void BeginReadColors();

#ifdef __cplusplus
}
#endif


#endif
