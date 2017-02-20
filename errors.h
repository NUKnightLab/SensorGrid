enum ERRORS {
     NO_ERR,
     MESSAGE_STRUCT_TOO_LARGE,
     LORA_INIT_FAIL,
     LORA_FREQ_FAIL,
     WIFI_MODULE_NOT_DETECTED
};

void fail(enum ERRORS err) {
    Serial.print(F("ERR: "));
    Serial.println(err);
    while(1);
}
