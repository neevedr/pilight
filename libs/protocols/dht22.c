/*
	Copyright (C) 2013 CurlyMo

	This file is part of pilight.

<<<<<<< HEAD
	pilight is free software: you can redistribute it and/or modify it under the 
	terms of the GNU General Public License as published by the Free Software 
	Foundation, either version 3 of the License, or (at your option) any later 
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY 
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR 
=======
	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
>>>>>>> upstream/development
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
<<<<<<< HEAD
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#include "../../pilight.h"
#include "common.h"
=======
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
>>>>>>> upstream/development
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "json.h"
#include "dht22.h"
<<<<<<< HEAD
#include "../pilight/wiringPi.h"
	
<<<<<<< HEAD
#define MAXTIMINGS 100

unsigned short dht22_loop = 1;
int dht22_nrfree = 0;
=======
#define MAXTIMINGS 85

unsigned short dht22_loop = 1;
>>>>>>> origin/master

static uint8_t sizecvt(const int read_value)
{
	/* digitalRead() and friends from wiringpi are defined as returning a value
	   < 256. However, they are returned as int() types. This is a safety function */
	if(read_value > 255 || read_value < 0) {
		logprintf(LOG_NOTICE, "invalid data from wiringPi library");
	}
  
	return (uint8_t)read_value;
}

void *dht22Parse(void *param) {

	struct JsonNode *json = (struct JsonNode *)param;
	struct JsonNode *jsettings = NULL;
<<<<<<< HEAD
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	int *id = 0;
	int nrid = 0, y = 0, interval = 5, x = 0;
	int temp_corr = 0, humi_corr = 0;
	int itmp;
=======
#include "../pilight/wiringX.h"

#define MAXTIMINGS 100

static unsigned short dht22_loop = 1;
static unsigned short dht22_threads = 0;

static pthread_mutex_t dht22lock;
static pthread_mutexattr_t dht22attr;

static uint8_t sizecvt(const int read_value) {
	/* digitalRead() and friends from wiringx are defined as returning a value
	   < 256. However, they are returned as int() types. This is a safety function */
	if(read_value > 255 || read_value < 0) {
		logprintf(LOG_NOTICE, "invalid data from wiringX library");
	}

	return (uint8_t)read_value;
}

static void *dht22Parse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	int *id = 0;
	int nrid = 0, y = 0, interval = 10, nrloops = 0;
	double temp_offset = 0.0, humi_offset = 0.0, itmp = 0.0;

	dht22_threads++;
>>>>>>> upstream/development

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "gpio", &itmp) == 0) {
				id = realloc(id, (sizeof(int)*(size_t)(nrid+1)));
<<<<<<< HEAD
				id[nrid] = itmp;
=======
				id[nrid] = (int)round(itmp);
>>>>>>> upstream/development
				nrid++;
			}
			jchild = jchild->next;
		}
	}
<<<<<<< HEAD
	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
		json_find_number(jsettings, "temp-corr", &temp_corr);
		json_find_number(jsettings, "humi-corr", &humi_corr);
	}
	json_delete(json);
	
	dht22_nrfree++;	

	while(dht22_loop) {
		for(y=0;y<nrid;y++) {
			int tries = 5;
			unsigned short got_correct_date = 0;
			while(tries && !got_correct_date && dht22_loop) {

				uint8_t laststate = HIGH;
				uint8_t counter = 0;
				uint8_t j = 0, i = 0;

				int dht22_dat[5] = {0,0,0,0,0};

				// pull pin down for 18 milliseconds
				pinMode(id[y], OUTPUT);			
				digitalWrite(id[y], HIGH);
				usleep(500000);  // 500 ms
				// then pull it up for 40 microseconds
				digitalWrite(id[y], LOW);
				usleep(20000);
				// prepare to read the pin
				pinMode(id[y], INPUT);

				// detect change and read data
				for(i=0; i<MAXTIMINGS; i++) {
					counter = 0;
					delayMicroseconds(10);
					while(sizecvt(digitalRead(id[y])) == laststate && dht22_loop) {
						counter++;
						delayMicroseconds(1);
						if(counter == 255) {
							break;
						}
					}
					laststate = sizecvt(digitalRead(id[y]));

					if(counter == 255) 
						break;

					// ignore first 3 transitions
					if((i >= 3) && (i%2 == 0)) {
					
						// shove each bit into the storage bytes
						dht22_dat[j/8] <<= 1;
						if(counter > 16)
							dht22_dat[j/8] |= 1;
						j++;
					}
				}

				// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
				// print it out if data is good
				if((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF))) {
					got_correct_date = 1;

					int h = dht22_dat[0] * 256 + dht22_dat[1];
					int t = (dht22_dat[2] & 0x7F)* 256 + dht22_dat[3];
					t += temp_corr;
					h += humi_corr;

					if((dht22_dat[2] & 0x80) != 0) 
						t *= -1;

					dht22->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "gpio", json_mknumber(id[y]));
					json_append_member(code, "temperature", json_mknumber(t));
					json_append_member(code, "humidity", json_mknumber(h));

					json_append_member(dht22->message, "code", code);
					json_append_member(dht22->message, "origin", json_mkstring("receiver"));
					json_append_member(dht22->message, "protocol", json_mkstring(dht22->id));

					pilight.broadcast(dht22->id, dht22->message);
					json_delete(dht22->message);
					dht22->message = NULL;
				} else {
					logprintf(LOG_DEBUG, "dht22 data checksum was wrong");
					tries--;
					for(x=0;x<1000;x++) {
						if(dht22_loop) {
							usleep((__useconds_t)(x));
						}
					}
				}
			}
		}
		for(x=0;x<(interval*1000);x++) {
			if(dht22_loop) {
				usleep((__useconds_t)(x));
			}
		}
	}

	if(id) sfree((void *)&id);
	dht22_nrfree--;

=======
	int interval = 5;
	int dht_pin = 7;

	json_find_number(json, "gpio", &dht_pin);
	if((jsettings = json_find_member(json, "settings"))) {
		json_find_number(jsettings, "interval", &interval);
	}
	json_delete(json);	
	
	while(dht22_loop) {

		int tries = 5;
		unsigned short got_correct_date = 0;

		while(tries && !got_correct_date) {

			uint8_t laststate = HIGH;
			uint8_t counter = 0;
			uint8_t j = 0, i = 0;

			int dht22_dat[5] = {0,0,0,0,0};

			// pull pin down for 18 milliseconds
			pinMode(dht_pin, OUTPUT);
			digitalWrite(dht_pin, LOW);
			delay(18);
			// then pull it up for 40 microseconds
			digitalWrite(dht_pin, HIGH);
			delayMicroseconds(40);
			// prepare to read the pin
			pinMode(dht_pin, INPUT);

			// detect change and read data
			for(i=0; i<MAXTIMINGS; i++) {
				counter = 0;
				while(sizecvt(digitalRead(dht_pin)) == laststate) {
					counter++;
					delayMicroseconds(1);
					if (counter == 255) {
						break;
					}
				}
				laststate = sizecvt(digitalRead(dht_pin));

				if(counter == 255) 
					break;

				// ignore first 3 transitions
				if((i >= 4) && (i%2 == 0)) {
					// shove each bit into the storage bytes
					dht22_dat[j/8] <<= 1;
					if (counter > 16)
						dht22_dat[j/8] |= 1;
						j++;
				  	}
				}

			// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
			// print it out if data is good
			if((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF))) {

				got_correct_date = 1;

				int h = dht22_dat[0] * 256 + dht22_dat[1];
				int t = (dht22_dat[2] & 0x7F)* 256 + dht22_dat[3];

				if((dht22_dat[2] & 0x80) != 0) 
					t *= -1;

				
				dht22->message = json_mkobject();
				JsonNode *code = json_mkobject();
				json_append_member(code, "id", json_mkstring(dht22->id));
				json_append_member(code, "temperature", json_mknumber(t));
				json_append_member(code, "humidity", json_mknumber(h));

				json_append_member(dht22->message, "code", code);
				json_append_member(dht22->message, "origin", json_mkstring("receiver"));
				json_append_member(dht22->message, "protocol", json_mkstring(dht22->id));
				pilight.broadcast(dht22->id, dht22->message);
				json_delete(dht22->message);
				dht22->message = NULL;
			} else {
				logprintf(LOG_DEBUG, "dht22 data checksum was wrong");
				tries--;
				sleep(1);
			}
		}
		sleep((unsigned int)interval);
	}

>>>>>>> origin/master
	return (void *)NULL;
}

void dht22InitDev(JsonNode *jdevice) {
<<<<<<< HEAD

#if defined(HARDWARE_433_GPIO) || defined(HARDWARE_433_PILIGHT)
	JsonNode *jid = NULL;
	JsonNode *jchild = NULL;
	int gpio_in = GPIO_IN_PIN;
	int gpio_out = GPIO_OUT_PIN;
	char *hw_mode = NULL;
	int free_hw_mode = 0;
	char **id = NULL;
	int nrid = 0, y = 0;
	char *stmp = NULL;

	settings_find_number("gpio-receiver", &gpio_in);
	settings_find_number("gpio-sender", &gpio_out);

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "gpio", &stmp) == 0) {
				id = realloc(id, (sizeof(char *)*(size_t)(nrid+1)));
				id[nrid] = malloc(strlen(stmp)+1);
				strcpy(id[nrid], stmp);
				nrid++;
			}
			jchild = jchild->next;
		}
	}
	
	if(settings_find_string("hw-mode", &hw_mode) != 0) {
		hw_mode = malloc(strlen(HW_MODE)+1);
		strcpy(hw_mode, HW_MODE);
		free_hw_mode = 1;
	}
	
	for(y=0;y<nrid;y++) {
		if(strstr(progname, "daemon") != 0 && strcmp(hw_mode, "gpio") == 0 && (atoi(id[y]) == gpio_in || atoi(id[y]) == gpio_out)) {
			logprintf(LOG_ERR, "dht22: gpio's already in use");
			goto clear;
		}
	}
#endif

=======
>>>>>>> origin/master
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	threads_register("dht22", &dht22Parse, (void *)json);
	sfree((void *)&output);
<<<<<<< HEAD

#ifdef HARDWARE_433_GPIO	
clear:
	if(free_hw_mode) sfree((void *)&hw_mode);
	if(id) sfree((void *)&id);
#endif
=======
>>>>>>> origin/master
}

int dht22GC(void) {
	dht22_loop = 0;
<<<<<<< HEAD
	while(dht22_nrfree > 0) {
		usleep(100);
	}
=======
>>>>>>> origin/master
	return 1;
}

void dht22Init(void) {
	gc_attach(dht22GC);

	protocol_register(&dht22);
	protocol_set_id(dht22, "dht22");
	protocol_device_add(dht22, "dht22", "1-wire temperature and humidity sensor");
<<<<<<< HEAD
=======
	protocol_device_add(dht22, "dht11", "1-wire temperature and humidity sensor");
>>>>>>> origin/master
	protocol_device_add(dht22, "am2302", "1-wire temperature and humidity sensor");	
	dht22->devtype = WEATHER;
	dht22->hwtype = SENSOR;

	options_add(&dht22->options, 't', "temperature", has_value, config_value, "^[0-9]{1,3}$");
	options_add(&dht22->options, 'h', "humidity", has_value, config_value, "^[0-9]{1,3}$");
<<<<<<< HEAD
	options_add(&dht22->options, 'g', "gpio", has_value, config_id, "^([0-9]{1}|1[0-9]|20)$");
=======
	options_add(&dht22->options, 'i', "id", has_value, config_id, ".+");
	options_add(&dht22->options, 'g', "gpio", has_value, config_value, "^[0-9]{1,2}$");
>>>>>>> origin/master

	protocol_setting_add_number(dht22, "decimals", 1);
	protocol_setting_add_number(dht22, "humidity", 1);
	protocol_setting_add_number(dht22, "temperature", 1);
	protocol_setting_add_number(dht22, "battery", 0);
	protocol_setting_add_number(dht22, "interval", 5);

	dht22->initDev=&dht22InitDev;
	
	wiringPiSetup();
}
=======

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	json_find_number(json, "temperature-offset", &temp_offset);
	json_find_number(json, "humidity-offset", &humi_offset);

	while(dht22_loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&dht22lock);
			for(y=0;y<nrid;y++) {
				int tries = 5;
				unsigned short got_correct_date = 0;
				while(tries && !got_correct_date && dht22_loop) {

					uint8_t laststate = HIGH;
					uint8_t counter = 0;
					uint8_t j = 0, i = 0;

					int dht22_dat[5] = {0,0,0,0,0};

					// pull pin down for 18 milliseconds
					pinMode(id[y], OUTPUT);
					digitalWrite(id[y], HIGH);
					usleep(500000);  // 500 ms
					// then pull it up for 40 microseconds
					digitalWrite(id[y], LOW);
					usleep(20000);
					// prepare to read the pin
					pinMode(id[y], INPUT);

					// detect change and read data
					for(i=0; (i<MAXTIMINGS && dht22_loop); i++) {
						counter = 0;
						delayMicroseconds(10);
						while(sizecvt(digitalRead(id[y])) == laststate && dht22_loop) {
							counter++;
							delayMicroseconds(1);
							if(counter == 255) {
								break;
							}
						}
						laststate = sizecvt(digitalRead(id[y]));

						if(counter == 255)
							break;

						// ignore first 3 transitions
						if((i >= 4) && (i%2 == 0)) {

							// shove each bit into the storage bytes
							dht22_dat[(int)((double)j/8)] <<= 1;
							if(counter > 16)
								dht22_dat[(int)((double)j/8)] |= 1;
							j++;
						}
					}

					// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
					// print it out if data is good
					if((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF))) {
						got_correct_date = 1;

						double h = dht22_dat[0] * 256 + dht22_dat[1];
						double t = (dht22_dat[2] & 0x7F)* 256 + dht22_dat[3];
						t += temp_offset;
						h += humi_offset;

						if((dht22_dat[2] & 0x80) != 0)
							t *= -1;

						dht22->message = json_mkobject();
						JsonNode *code = json_mkobject();
						json_append_member(code, "gpio", json_mknumber(id[y], 0));
						json_append_member(code, "temperature", json_mknumber(t/10, 1));
						json_append_member(code, "humidity", json_mknumber(h/10, 1));

						json_append_member(dht22->message, "message", code);
						json_append_member(dht22->message, "origin", json_mkstring("receiver"));
						json_append_member(dht22->message, "protocol", json_mkstring(dht22->id));

						pilight.broadcast(dht22->id, dht22->message);
						json_delete(dht22->message);
						dht22->message = NULL;
					} else {
						logprintf(LOG_DEBUG, "dht22 data checksum was wrong");
						tries--;
						protocol_thread_wait(node, 1, &nrloops);
					}
				}
			}
			pthread_mutex_unlock(&dht22lock);
		}
	}
	pthread_mutex_unlock(&dht22lock);

	sfree((void *)&id);
	dht22_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *dht22InitDev(JsonNode *jdevice) {
	dht22_loop = 1;
	wiringXSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(dht22, json);
	return threads_register("dht22", &dht22Parse, (void *)node, 0);
}

static void dht22ThreadGC(void) {
	dht22_loop = 0;
	protocol_thread_stop(dht22);
	while(dht22_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(dht22);
}

#ifndef MODULE
__attribute__((weak))
#endif
void dht22Init(void) {
	pthread_mutexattr_init(&dht22attr);
	pthread_mutexattr_settype(&dht22attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&dht22lock, &dht22attr);

	protocol_register(&dht22);
	protocol_set_id(dht22, "dht22");
	protocol_device_add(dht22, "dht22", "1-wire Temperature and Humidity Sensor");
	protocol_device_add(dht22, "am2302", "1-wire Temperature and Humidity Sensor");
	dht22->devtype = WEATHER;
	dht22->hwtype = SENSOR;

	options_add(&dht22->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&dht22->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&dht22->options, 'g', "gpio", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|1[0-9]|20)$");

	// options_add(&dht22->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&dht22->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&dht22->options, 0, "humidity-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&dht22->options, 0, "decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&dht22->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&dht22->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&dht22->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	dht22->initDev=&dht22InitDev;
	dht22->threadGC=&dht22ThreadGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "dht22";
	module->version = "1.5";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	dht22Init();
}
#endif
>>>>>>> upstream/development
