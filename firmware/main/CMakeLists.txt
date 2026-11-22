# idf_component_register(SRCS
#                     INCLUDE_DIRS "."
#                     REQUIRES driver esp_driver_i2s esp_driver_gpio)

idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
	# "notas/OndasBasicas"        # ondas básicas deshabilitadas por ahora
    "notas/SalamanderGrandPiano"
    # "notas/flauta"            # flauta y violín fuera del repositorio
	"notas/guitarra"
	# "notas/violin"
	"canciones"
	"tasks"
    REQUIRES
        driver
        esp_driver_gptimer 
        esp_driver_i2s
        esp_driver_dac
        esp_driver_gpio   
        freertos
        esp_timer
        u8g2
        led_strip
        fatfs 
        sdmmc
)