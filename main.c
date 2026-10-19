/*
 * Nucleo STM32F401RE + Zephyr
 * Productor / Consumidor con hilos, cola de mensajes y semáforo.
 *
 *   [Hilo productor] --(k_msgq)--> [Hilo consumidor] --UART--> ESP32-C3
 *
 * - Productor: cada segundo "mide" un dato y lo pone en la cola.
 * - Consumidor: espera datos en la cola, arma el JSON y lo envía por USART1.
 * - Semáforo: protege la consola (printk), que la usan los dos hilos.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>

/* ---------- Recursos del RTOS ---------- */

/* Dato que viaja por la cola */
struct dato {
	int cont;
	int temp;
};

/* Cola de mensajes: hasta 5 datos de tipo struct dato */
K_MSGQ_DEFINE(cola_datos, sizeof(struct dato), 5, 4);

/* Semáforo binario (valor inicial 1, máximo 1) para usar la consola */
K_SEM_DEFINE(sem_consola, 1, 1);

/* UART que va hacia el ESP32-C3 (definida en el overlay) */
static const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(usart1));

/* ---------- Hilo productor ---------- */
void productor(void *a, void *b, void *c)
{
	struct dato d = { .cont = 0, .temp = 25 };

	while (1) {
		d.cont++;
		d.temp = 25 + (d.cont % 5);   /* Simulamos un sensor */

		/* Ponemos el dato en la cola (espera si está llena) */
		k_msgq_put(&cola_datos, &d, K_FOREVER);

		k_sem_take(&sem_consola, K_FOREVER);
		printk("[Productor] dato %d puesto en la cola\n", d.cont);
		k_sem_give(&sem_consola);

		k_msleep(1000);
	}
}

/* ---------- Hilo consumidor ---------- */
void consumidor(void *a, void *b, void *c)
{
	struct dato d;
	char json[64];

	if (!device_is_ready(uart)) {
		printk("Error: USART1 no esta lista\n");
		return;
	}

	while (1) {
		/* Esperamos (bloqueados) hasta que llegue un dato a la cola */
		k_msgq_get(&cola_datos, &d, K_FOREVER);

		/* Armamos el JSON. El '\n' marca el fin del mensaje */
		snprintf(json, sizeof(json),
			 "{\"cont\":%d,\"temp\":%d}\n", d.cont, d.temp);

		/* Enviamos carácter por carácter por la UART */
		for (char *p = json; *p; p++) {
			uart_poll_out(uart, *p);
		}

		k_sem_take(&sem_consola, K_FOREVER);
		printk("[Consumidor] enviado: %s", json);
		k_sem_give(&sem_consola);
	}
}

/* ---------- Creación de los hilos ----------
 * nombre, tamaño de pila, función, 3 argumentos, prioridad, opciones, retardo
 */
K_THREAD_DEFINE(hilo_prod, 1024, productor, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(hilo_cons, 1024, consumidor, NULL, NULL, NULL, 5, 0, 0);

/* main no hace nada: los hilos arrancan solos */
int main(void)
{
	return 0;
}
