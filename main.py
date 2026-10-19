# ESP32-C3 mini + MicroPython
# Recibe {"cont":N,"temp":24.56,"volt":1234} desde la Nucleo.
# Productor / Consumidor con la librería _thread.
#
#   UART --> [Hilo productor] --(cola + lock)--> [Hilo consumidor] --> consola y OLED
#
# - Productor: lee líneas JSON de la UART y las pone en la cola.
# - Consumidor: saca datos de la cola y los muestra.
# - Lock: evita que los dos hilos usen la cola al mismo tiempo.
 
from machine import UART, Pin, I2C
import _thread
import json
import time
import ssd1306
 
# --- Hardware ---
uart = UART(1, baudrate=115200, rx=4, tx=5)     # RX = GPIO4 <- TX de la Nucleo (D8)
i2c = I2C(0, sda=Pin(8), scl=Pin(9))
oled = ssd1306.SSD1306_I2C(128, 64, i2c)
 
# --- Recursos compartidos ---
cola = []                         # Lista usada como cola
candado = _thread.allocate_lock() # Lock (semáforo binario)
 
 
def productor():
    while True:
        linea = uart.readline()          # Lee hasta el '\n'
        if linea:
            try:
                datos = json.loads(linea)    # Texto JSON -> diccionario
                with candado:                # Toma el lock ... lo suelta al salir
                    cola.append(datos)
            except ValueError:
                print("Linea invalida:", linea)
        time.sleep_ms(10)
 
 
def consumidor():
    while True:
        datos = None
        with candado:
            if cola:
                datos = cola.pop(0)      # Saca el dato más antiguo
 
        if datos:
            print("Recibido:", datos)
            oled.fill(0)
            oled.text("Datos Nucleo", 0, 0)
            oled.text("Cont: " + str(datos["cont"]), 0, 16)
            oled.text("Temp: " + str(datos["temp"]) + " C", 0, 32)
            oled.text("Volt: " + str(datos["volt"]) + " mV", 0, 48)
            oled.show()
        time.sleep_ms(50)
 
 
# Pantalla inicial
oled.fill(0)
oled.text("Esperando...", 0, 0)
oled.show()
 
# Arrancamos los dos hilos      
_thread.start_new_thread(productor, ())
_thread.start_new_thread(consumidor, ())
 
# El hilo principal solo se queda vivo
while True:
    time.sleep(1)
