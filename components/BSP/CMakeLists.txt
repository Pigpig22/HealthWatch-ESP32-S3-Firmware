set(src_dirs 
            WIFI
            RTC_TIME
            EXIT_KEY 
            XL9555
            MAX30102
            MPU6050
            FALL_DETECT
            NTC
            BLE
            OLED)
set(include_dirs 
            WIFI
            RTC_TIME
            EXIT_KEY
            XL9555
            MAX30102
            MPU6050
            FALL_DETECT
            NTC
            BLE
            OLED)

# 这里是重点：必须加上 esp_wifi esp_event nvs_flash 等依赖
set(requires 
            driver
            esp_wifi
            esp_event
            nvs_flash
            bt
            log
)

idf_component_register(SRC_DIRS ${src_dirs}
                    INCLUDE_DIRS ${include_dirs}
                    REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)