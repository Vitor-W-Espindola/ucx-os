#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

pthread_t threads[5];
const int msec_min = 10;
const int msec_max = 50;

/* Binary Semaphores:
 *	s_luminosity_adc: locks task_head until luminosity_adc gets 
 *			its ADC value. This mutex is initialized
 *			as locked.
 *	s_temperature_adc: locks task_head until temperature_adc gets 
 *			its ADC value. This mutex is intialized
 *			as locked. 
 * 	s_luminosity_pwm: it is locked until task_head gets the ADC value
 * 			from mt_luminosity_adc. This mutex is initialized
 * 			as locked.
 * 	s_temperature_pwm: it is locked until task_head gets the ADC value
 * 			from mt_temperature_adc. This mutex is initialized
 * 			as locked.
 * 	s_adc:	it is used inside mt_luminosity_adc and mt_temperature_adc
 * 			for protecting the concurrent usage of 
 * 			the same ADC channel
 * 	s_pwm: it is used inside mt_luminosity_pwm and mt_temperature_pwm
 * 			for protecting the concurrent usage of
 * 			the same PWM channel
 *
 * */
sem_t s_luminosity_adc, s_temperature_adc, s_luminosity_pwm, s_temperature_pwm, s_adc, s_pwm;

void *task_head(void *arg) {
	const int msec = msec_min + (rand() % msec_max); // [msec_min ; msec_max] ms
	struct timespec ts = { .tv_sec = msec / 1000, .tv_nsec = (msec % 1000) * 1000000 };

	while(1) {
		printf("task_head: waiting for luminosity adc\n");
		sem_wait(&s_luminosity_adc);
		printf("task_head: got luminosity adc value\n"); 

		printf("task_head: posting for luminosity pwm\n");
		sem_post(&s_luminosity_pwm);

		printf("task_head: waiting for temperature adc\n");
		sem_wait(&s_temperature_adc);
		printf("task_head: got temperature adc value\n");
		
		printf("task_head: posting for temperature pwm\n");
		sem_post(&s_temperature_pwm);

		nanosleep(&ts, &ts);
	}	
};

void *luminosity_adc(void *arg) {
	const int msec = msec_min + (rand() % msec_max);
	struct timespec ts = { .tv_sec = msec / 1000, .tv_nsec = (msec % 1000) * 1000000 };

	while(1) {
		printf("luminosity_adc: waiting s_adc\n");
		sem_wait(&s_adc);
		printf("luminosity_adc: posting s_adc\n");
		sem_post(&s_adc);

		printf("luminosity_adc: posting s_luminosity_adc\n");
		sem_post(&s_luminosity_adc);
		
		nanosleep(&ts, &ts);
	}
};

void *temperature_adc(void *arg) {
	const int msec = msec_min + (rand() % msec_max);
	struct timespec ts = { .tv_sec = msec / 1000, .tv_nsec = (msec % 1000) * 1000000 };
	
	while(1) {
		printf("temperature_adc: waiting s_adc\n");
		sem_wait(&s_adc);
		printf("temperature_adc posting s_adc\n");
		sem_post(&s_adc);

		printf("temperature_adc: posting s_temperature_adc\n");
		sem_post(&s_temperature_adc);

		nanosleep(&ts, &ts);
	}
};

void *luminosity_pwm(void *arg) {
	const int msec = msec_min + (rand() % msec_max);
	struct timespec ts = { .tv_sec = msec / 1000, .tv_nsec = (msec % 1000) * 1000000 };

	while(1) { 
		printf("luminosity_pwm: waiting s_luminosity_pwm\n");
		sem_wait(&s_luminosity_pwm);

		printf("luminosity_pwm: waiting s_pwm\n");
		sem_wait(&s_pwm);
		printf("luminosity_pwm: posting s_pwm\n");
		sem_post(&s_pwm);
		
		nanosleep(&ts, &ts);
	}
};

void *temperature_pwm(void *arg) {
	const int msec = msec_min + (rand() % msec_max);
	struct timespec ts = { .tv_sec = msec / 1000, .tv_nsec = (msec % 1000) * 1000000 };
	
	while(1) {
		printf("temperature_pwm: waiting s_temperature_pwm\n");
		sem_wait(&s_temperature_pwm);

		printf("temperature_pwm: waiting s_pwm\n");
		sem_wait(&s_pwm);
		printf("temperature_pwm: posting s_pwm\n");
		sem_post(&s_pwm);
		
		nanosleep(&ts, &ts);
	}
};

int main() {
	srand(time(NULL));
	
	/* Binary Semaphores initialization */
	sem_init(&s_luminosity_adc, 0, 0);
	sem_init(&s_temperature_adc, 0, 0);
	sem_init(&s_luminosity_pwm, 0, 0);
	sem_init(&s_temperature_pwm, 0, 0);
	sem_init(&s_adc, 0, 1);
	sem_init(&s_pwm, 0, 1);

	/* Threads creation */
	pthread_create(&(threads[0]), NULL, task_head, NULL);
	pthread_create(&(threads[1]), NULL, luminosity_adc, NULL);
	pthread_create(&(threads[2]), NULL, temperature_adc, NULL);
	pthread_create(&(threads[3]), NULL, luminosity_pwm, NULL);
	pthread_create(&(threads[4]), NULL, temperature_pwm, NULL);
	
	/* Waiting for children */
	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);
	pthread_join(threads[2], NULL);
	pthread_join(threads[3], NULL);
	pthread_join(threads[4], NULL);


	return 0;
}
