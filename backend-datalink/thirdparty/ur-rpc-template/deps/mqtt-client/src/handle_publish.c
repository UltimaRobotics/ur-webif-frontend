/*
Copyright (c) 2009-2018 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
   http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

Contributors:
   Roger Light - initial implementation and documentation.
*/

#include "config.h"

#include <assert.h>
#include <string.h>
#include <time.h>

#include "mosquitto.h"
#include "mosquitto_internal.h"
#include "logging_mosq.h"
#include "memory_mosq.h"
#include "messages_mosq.h"
#include "packet_mosq.h"
#include "send_mosq.h"
#include "time_mosq.h"


int handle__publish(struct mosquitto *mosq)
{
	uint8_t header;
	struct mosquitto_message_all *message;
	int rc = 0;
	uint16_t mid;
	int slen;

	assert(mosq);

	message = mosquitto__calloc(1, sizeof(struct mosquitto_message_all));
	if(!message) return MOSQ_ERR_NOMEM;

	header = mosq->in_packet.command;

	message->dup = (header & 0x08)>>3;
	message->msg.qos = (header & 0x06)>>1;
	message->msg.retain = (header & 0x01);

	rc = packet__read_string(&mosq->in_packet, &message->msg.topic, &slen);
	if(rc){
		message__cleanup(&message);
		return rc;
	}
	if(!slen){
		message__cleanup(&message);
		return MOSQ_ERR_PROTOCOL;
	}

	if(message->msg.qos > 0){
		rc = packet__read_uint16(&mosq->in_packet, &mid);
		if(rc){
			message__cleanup(&message);
			return rc;
		}
		message->msg.mid = (int)mid;
	}

	message->msg.payloadlen = mosq->in_packet.remaining_length - mosq->in_packet.pos;
	if(message->msg.payloadlen){
		message->msg.payload = mosquitto__calloc(message->msg.payloadlen+1, sizeof(uint8_t));
		if(!message->msg.payload){
			message__cleanup(&message);
			return MOSQ_ERR_NOMEM;
		}
		rc = packet__read_bytes(&mosq->in_packet, message->msg.payload, message->msg.payloadlen);
		if(rc){
			message__cleanup(&message);
			return rc;
		}
	}
	log__printf(mosq, MOSQ_LOG_DEBUG,
			"Client %s received PUBLISH (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))",
			mosq->id, message->dup, message->msg.qos, message->msg.retain,
			message->msg.mid, message->msg.topic,
			(long)message->msg.payloadlen);

	message->timestamp = mosquitto_time();
	
	/* Deduplication: Track last processed message to prevent duplicate callbacks */
	static uint16_t last_mid = 0;
	static time_t last_time = 0;
	static char last_topic[256] = {0};
	
	time_t current_time = time(NULL);
	
	/* Check if this is a duplicate message (same mid, topic, within 2 seconds) */
	if(message->msg.qos > 0 && 
	   message->msg.mid == last_mid && 
	   strcmp(message->msg.topic, last_topic) == 0 &&
	   (current_time - last_time) < 2) {
		/* Duplicate detected - send ACK but don't invoke callback */
		log__printf(mosq, MOSQ_LOG_DEBUG,
				"Client %s ignoring duplicate PUBLISH (mid=%d, topic='%s')",
				mosq->id, message->msg.mid, message->msg.topic);
		
		if(message->msg.qos == 1){
			rc = send__puback(mosq, message->msg.mid);
			message__cleanup(&message);
			return rc;
		}else if(message->msg.qos == 2){
			rc = send__pubrec(mosq, message->msg.mid);
			message__cleanup(&message);
			return rc;
		}
	}
	
	/* Update deduplication tracking for QoS > 0 messages */
	if(message->msg.qos > 0) {
		last_mid = message->msg.mid;
		last_time = current_time;
		strncpy(last_topic, message->msg.topic, sizeof(last_topic) - 1);
		last_topic[sizeof(last_topic) - 1] = '\0';
	}
	
	switch(message->msg.qos){
		case 0:
			pthread_mutex_lock(&mosq->callback_mutex);
			if(mosq->on_message){
				mosq->in_callback = true;
				mosq->on_message(mosq, mosq->userdata, &message->msg);
				mosq->in_callback = false;
			}
			pthread_mutex_unlock(&mosq->callback_mutex);
			message__cleanup(&message);
			return MOSQ_ERR_SUCCESS;
		case 1:
			rc = send__puback(mosq, message->msg.mid);
			pthread_mutex_lock(&mosq->callback_mutex);
			if(mosq->on_message){
				mosq->in_callback = true;
				mosq->on_message(mosq, mosq->userdata, &message->msg);
				mosq->in_callback = false;
			}
			pthread_mutex_unlock(&mosq->callback_mutex);
			message__cleanup(&message);
			return rc;
		case 2:
			rc = send__pubrec(mosq, message->msg.mid);
			pthread_mutex_lock(&mosq->in_message_mutex);
			message->state = mosq_ms_wait_for_pubrel;
			message__queue(mosq, message, mosq_md_in);
			pthread_mutex_unlock(&mosq->in_message_mutex);
			return rc;
		default:
			message__cleanup(&message);
			return MOSQ_ERR_PROTOCOL;
	}
}

