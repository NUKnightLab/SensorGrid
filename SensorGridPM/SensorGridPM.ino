/**
 * Knight Lab SensorGrid
 *
 * Wireless air pollution (Particulate Matter PM2.5 & PM10) monitoring over LoRa radio
 */
#include "config.h"
#include "lora.h"
#include "runtime.h"
#include "oled.h"

#define SET_CLOCK false

enum Mode mode = WAIT;

RTCZero rtcz;
RTC_PCF8523 rtc;
OLED oled = OLED(rtc);



uint32_t sampleRate = 10; //sample rate of the sine wave in Hertz, how many times per second the TC5_Handler() function gets called per second basically
int MAX_TIMEOUT = 10;
//unsigned long sample_period = 60 * 10;
//unsigned long heartbeat_period = 30;
unsigned long next_sample;
int sensor_init_time = 7;

/* local utilities */

static void setup_logging()
{
    Serial.begin(115200);
    set_logging(true);
}

static void set_rtcz()
{
    DateTime dt = rtc.now();
    rtcz.setDate(dt.day(), dt.month(), dt.year());
    rtcz.setTime(dt.hour(), dt.minute(), dt.second());
}

static void current_time()
{
    Serial.print("Current time: ");
    Serial.print(rtcz.getHours());
    Serial.print(":");
    Serial.print(rtcz.getMinutes());
    Serial.print(":");
    Serial.println(rtcz.getSeconds());
    Serial.println(rtcz.getEpoch());
}

uint32_t get_time()
{
    return rtcz.getEpoch();
}

/* end local utilities */

/*
 * interrupts
 */

void aButton_ISR()
{
    static bool busy = false;
    if (busy) return;
    busy = true;
    rtcz.disableAlarm();
    static volatile int state = 0;
    state = !digitalRead(BUTTON_A);\
    if (state) {
        Serial.println("A-Button pushed");
        oled.toggleDisplayState();
    }
    busy = false;
    //rtcz.disableAlarm();
    /*
    aButtonState = !digitalRead(BUTTON_A);
    if (aButtonState) {
        _on = !_on;
        if (_on) {
            _activated_time = millis();
            displayDateTimeOLED();
        } else {
            standbyOLED();
        }
    }
    */
}


/* low power configuration */
/*
void Low_Power_Config(void)
{
  // Enable deep sleep mode
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
 
  // Enable OSCERCLK in STOP mode
  OSC0->CR |= OSC_CR_EREFSTEN_MASK;
  // Need this for UART and Low Power Timer to continue
 
  // Switch system to run at 1MHz
  SIM->CLKDIV1 = SIM_CLKDIV1_OUTDIV1(7)|SIM_CLKDIV1_OUTDIV4(7);
  // Turn off flash during sleep (Flash Doze)
  SIM->FCFG1 |= SIM_FCFG1_FLASHDOZE_MASK;
 
  MCG->C2 |= MCG_C2_LP_MASK; // Low Power Select
  //Controls whether the FLL or PLL is disabled in 
   //BLPI and BLPE modes. In FBE or PBE modes, setting this
  //bit to 1 will transition the MCG into BLPE mode; 
  //in FBI mode, setting this bit to 1 will transition the MCG
  //into BLPI mode. In any other MCG mode, LP bit has no affect.
  //0 FLL or PLL is not disabled in bypass modes.
  //1 FLL or PLL is disabled in bypass modes (lower power)
 
  MCG->C2 &= ∼MCG_C2_HGO0_MASK;
  // Controls the crystal oscillator mode of operation. 
  // See the Oscillator (OSC) chapter for more details.
  // 0 Configure crystal oscillator for low-power operation.
  // 1 Configure crystal oscillator for high-gain operation.
  // Note: HGO0 of MCG->C2 might already be zero
  // Turn off internal reference clock, as we are
  // using external crystal
  MCG->C1 &= ∼MCG_C1_IRCLKEN_MASK;
 
  // Enable Very Low Power modes
  SMC->PMPROT |= SMC_PMPROT_AVLP_MASK;
  // Enable Very-Low-Power Run mode (VLPR)
  // and Very-Low-Power Stop (VLPS)
  SMC->PMCTRL = SMC_PMCTRL_RUNM(2) | // VLPR
                SMC_PMCTRL_STOPM(2); // VLPS
  printf ("Waiting to enter VLPR…\n");
  while ((SMC->PMSTAT & 0x7F)!=0x04);
  printf ("VLPR activated!\n");
  return;
}
*/

/* setup and loop */

void setup()
{
    rtc.begin();
    if (SET_CLOCK) {
        Serial.print("Printing initial DateTime: ");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        Serial.print(F(__DATE__));
        Serial.print(' ');
        Serial.println(F(__TIME__));
    }
    rtcz.begin();
    set_rtcz();
    //initOLED(rtc);
    oled.init();
    /* This is causing lock-up. Need to do further research into low power modes
       on the Cortex M0 */
    //oled.setButtonFunction(BUTTON_A, *aButton_ISR, CHANGE);
    oled.displayDateTime();
    //displayDateTimeOLED();

    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < WAIT_SERIAL_TIMEOUT );
    if (ALWAYS_LOG || Serial) {
        setup_logging();
    }
    logln(F("begin setup .."));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    loadConfig();
    setup_radio(config.RFM95_CS, config.RFM95_INT, config.node_id);
    radio->sleep();
    //startTimer(10);
    pinMode(12, OUTPUT); // enable pin to HPM boost
    HONEYWELL_HPM::setup(config.node_id, 0, &get_time);
    delay(2000);
    HONEYWELL_HPM::stop();
    logln(F(".. setup complete"));
    current_time();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    // This is done in RTCZero::standbyMode
    // https://github.com/arduino-libraries/RTCZero/blob/master/src/RTCZero.cpp
    // SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
}

void loop()
{
    static uint32_t start_time = get_time();
    static uint32_t next_collection_time = getNextCollectionTime();
    if (start_time && get_time() - start_time > 3 * 60)
        oled.off();
    if (mode == WAIT) {
        set_init_timeout();
    } else if (mode == INIT) {
        init_sensors();
        set_sample_timeout();
    } else if (mode == SAMPLE) {
        record_data_samples();
        if (get_time() > next_collection_time) {
            setCommunicateDataTimeout();
        } else {
            mode = WAIT;
        }
    } else if (mode == COMMUNICATE) {
        if (DO_LOG_DATA) {
            logData(!DO_TRANSMIT_DATA);
        }
        if (DO_TRANSMIT_DATA) {
            communicate_data();
        }
        next_collection_time = getNextCollectionTime();
        mode = WAIT;
    } else if (mode == HEARTBEAT) {
        flash_heartbeat();
        mode = WAIT;
    }
    oled.displayDateTime();
}
