idf_component_register(
    SRCS
        "bme680_test.c"
        "bme68x.c"
        "DFRobot_AirQualitySensor.c"
    INCLUDE_DIRS "."
    REQUIRES driver bme680 bsec esp_timer esp_adc
)


