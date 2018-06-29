#include "HONEYWELL_HPM.h"

byte enable_autosend[] = {0x68, 0x01, 0x40, 0x57};
byte stop_autosend[] = { 0x68, 0x01, 0x20, 0x77 };
byte start_pm[] = { 0x68, 0x01, 0x01, 0x96 };
byte stop_pm[] = { 0x68, 0x01, 0x02, 0x95 };
byte read_pm_results[] = { 0x68, 0x01, 0x04, 0x93 };
byte uartbuf[32];


/*
 * Read the next message from the sensor off the UART
 */
bool read_message(byte* buf)
{
    memset(buf, 0, 32);
    uint8_t i = 0;
    uint8_t len = 32;
    unsigned long activity_time;
    //while (i < len && (millis() - activity_time) < UART_TIMEOUT) {
    while (i < len) {
        activity_time = millis();
        while (!Serial1.available()) {
            if ( (millis() - activity_time) > UART_TIMEOUT) {
                return false;
            }
        }
        byte b = Serial1.read();
        Serial.print(b, HEX);
        //activity_time = millis();
        Serial.print(" ");
        if (i == 0) {
            if (b == 0x40 || b == 0xA5 || b == 0x96 || b == 0x42) {
                buf[i++] = b;
                switch (b) {
                    case 0x40:
                        len = 8; break;
                    case 0xA5:
                        len = 2; break;
                    case 0x96:
                        len = 2; break;
                    case 0x42:
                        len = 32; break;
                }
            }
        } else if (i == 1) {
            if (  (b == 0x05 && buf[0] == 0x40)
                  || (b == 0xA5 && buf[0] == 0xA5)
                  || (b == 0x96 && buf[0] == 0x96)
                  || (b == 0x4d && buf[0] == 0x42) ) {
                buf[i++] = b;
            } else {
                i = 0;
            }
        } else if (i == 2) {
            if ( (b != 0x04)
                 ||(buf[0] == 0x40 && buf[1] == 0x05) ) {
                buf[i++] = b;
            } else {
                i = 0;
            }
        } else {
            buf[i++] = b;
        }
    }
    for (int j=0; j<i; j++) {
        Serial.print(buf[j]); Serial.print(" ");
    }
    if (i == len) {
        return true;
    } else {
        return false;
    }
}

void _read_pm_results_data(int* pm25, int* pm10)
{
    int retries = 4;
    for (int i=0; i<retries; i++) {    
        Serial.print("Reading HPM data, attempt #");
        Serial.println(i+1, DEC);
        Serial.println("SENDING: READ_PARTICLE_MEASURING_RESULTS ..");
        Serial1.write(read_pm_results, 4);
        read_message(uartbuf);
        Serial.println("");
        *pm25 = uartbuf[3]*256 + uartbuf[4]; // PM 2.5
        *pm10 = uartbuf[5]*256+uartbuf[6]; // PM 10
        Serial.print("PM 2.5: "); Serial.println(*pm25);
        Serial.print("PM 10: "); Serial.println(*pm10);
        if (*pm25 != 7168) break;
        delay(250);
    }
}

void read_pm_results_data(int* pm25, int* pm10)
{
    int pm25_1, pm25_2, pm25_3, pm10_1, pm10_2, pm10_3;
    _read_pm_results_data(&pm25_1, &pm10_1);
    delay(250);
    _read_pm_results_data(&pm25_2, &pm10_2);
    delay(250);
    _read_pm_results_data(&pm25_3, &pm10_3);
    int pm25_mean = (pm25_1 + pm25_2 + pm25_3) / 3;
    int pm10_mean = (pm10_1 + pm10_2 + pm10_3) / 3;
    int pm25_std = sqrt( 
        (pow((pm25_1 - pm25_mean), 2) +
        pow((pm25_2 - pm25_mean), 2) +
        pow((pm25_3 - pm25_mean), 2)) / 3);
    int pm10_std = sqrt( 
        (pow((pm25_1 - pm25_mean), 2) +
        pow((pm25_2 - pm25_mean), 2) +
        pow((pm25_3 - pm25_mean), 2)) / 3);
    int total = 0;
    int count = 0;
    if (abs(pm25_1 - pm25_mean) <= 0.5 * pm25_std) {
        total += pm25_1;
        count++;
    }
    if (abs(pm25_2 - pm25_mean) <= 0.5 * pm25_std) {
        total += pm25_2;
        count++;
    }
    if (abs(pm25_3 - pm25_mean) <= 0.5 * pm25_std) {
        total += pm25_3;
        count++;
    }
    int val = total / count;
    if (count == 0) {
        *pm25 = (pm25_1 + pm25_2 + pm25_3) / 3;
    } else {
        *pm25 = total / count;
    }
    total = 0;
    count = 0;
    if (abs(pm10_1 - pm10_mean) <= 0.5 * pm10_std) {
        total += pm10_1;
        count++;
    }
    if (abs(pm10_2 - pm10_mean) <= 0.5 * pm10_std) {
        total += pm10_2;
        count++;
    }
    if (abs(pm10_3 - pm10_mean) <= 0.5 * pm10_std) {
        total += pm10_3;
        count++;
    }
    if (count == 0) {
        *pm10 = (pm10_1 + pm10_2 + pm10_3) / 3;
    } else {
        *pm10 = total / count;
    }
}

bool send_start_pm()
{
    Serial.print("SENDING: START_PARTICLE_MEASUREMENT ..");
    Serial1.write(start_pm, 4);
    if (read_message(uartbuf) && uartbuf[0] == 0xA5 && uartbuf[1] == 0xA5) {
        Serial.println(".. SENT");
        return true;
    } else {
        Serial.println("\nWARNING: Trouble sending START_PARTICLE_MEASUREMENT");
        return false;
    }
}


bool send_stop_pm()
{
    Serial1.write(stop_pm, 4);
    /* For some reason, the sensor is returning a data message instead of a STOP_PM ack. The
     *  following logic attempts to read out a subsequent STOP_PM ack with the assumption
     *  that the data just needs to be cleared out of the buffer -- but that does not seem
     *  to be the case. Rather, it seems that we are really getting a data payload instead of the ACK
     *
     *  TODO: verify that the fan is stopping consistently even if a data message is received
     *  instead of an ACK. If so, warning can be removed.
     */
    if (read_message(uartbuf)) {
        if(uartbuf[0] == 0xA5 && uartbuf[1] == 0xA5) {
            return true;
        } else if (uartbuf[0] == 0x40 && uartbuf[1] == 0x05) {
            if (read_message(uartbuf) && uartbuf[0] == 0xA5 && uartbuf[1] == 0xA5) {
                return true;
            }
        }
    }
    return false;
}

void send_stop_autosend()
{
    Serial.print("SENDING: STOP_AUTOSEND ..");
    Serial1.write(stop_autosend, 4);
    if (read_message(uartbuf) && uartbuf[0] == 0xA5 && uartbuf[1] == 0xA5) {
        Serial.println(".. SENT");
    } else {
        Serial.println("\nTrouble sending: STOP_AUTOSEND");
    }
}

static TimeFunction _time_fcn;
static uint8_t _node_id;

namespace HONEYWELL_HPM {

    bool setup(uint8_t node_id, TimeFunction time_fcn)
    {
        log_(F("Setup Honeywell HPM sensor .. "));
        _node_id = node_id;
        Serial1.begin(9600);
        _time_fcn = time_fcn;
        println(F("done"));
        return true;
    }

    bool start()
    {
        send_start_pm();
        delay(100);
        send_stop_autosend();
        return true;
    }

    /*size_t read(char* buf, int len)
    {
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        int pm25;
        int pm10;
        root["node"] = _node_id;
        root["ts"] = _time_fcn();
        read_pm_results_data(&pm25, &pm10);
        JsonArray& data = root.createNestedArray("hpm");
        data.add(pm25);
        data.add(pm10);
        root.printTo(Serial);
        Serial.println();
        return root.printTo(buf, len);
    }*/

    // void readData(JsonArray &data_array)
    // {
    //     StaticJsonBuffer<200> jsonBuffer;
    //     //JsonObject& root = jsonBuffer.createObject();
    //     JsonObject& root = data_array.createNestedObject();
    //     int pm25;
    //     int pm10;
    //     root["node"] = _node_id;
    //     root["ts"] = _time_fcn();
    //     read_pm_results_data(&pm25, &pm10);
    //     JsonArray& data = root.createNestedArray("hpm");
    //     data.add(pm25);
    //     data.add(pm10);
    //     root.printTo(Serial);
    //     Serial.println();
    //     //return root.printTo(buf, len);
    //     //data_array.add(root);
    // }

    size_t read(char* buf, int len)
    {
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        //JsonObject& root = data_array.createNestedObject();
        root["node"] = _node_id;
        root["ts"] = _time_fcn();
        int pm25;
        int pm10;
        //int count = 0;
        //int pm25total = 0;
        //int pm10total = 0;
        read_pm_results_data(&pm25, &pm10);
        //if (pm25 != 7168) {
        //    pm25total += pm25;
        //    pm10total += pm10;
        //    count++;
        //}
        //pm25 = pm25total / count;
        //pm10 = pm10total / count;
        JsonArray& data = root.createNestedArray("hpm");
        data.add(pm25);
        data.add(pm10);
        root.printTo(Serial);
        Serial.println();
        return root.printTo(buf, len);
        //data_array.add(root);
    }

    bool stop()
    {
        /* Sensor tends to miss a lot of stop commands, so we retry several
           times until we get a stop acknowledgment */
        Serial.print("SENDING: STOP_PARTICLE_MEASUREMENT ..");
        for (int i=0; i<10; i++) {
            if (send_stop_pm())
                Serial.println(".. SENT");
                return true;
            delay(100);
        }
        Serial.println("\nTrouble sending: STOP_PARTICLE_MEASUREMENT");
        Serial.println("WARNING: Sensor fan may not have stopped");
        return false;
    }
}
