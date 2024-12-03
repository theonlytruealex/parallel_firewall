// SPDX-License-Identifier: BSD-3-Clause

#include "ring_buffer.h"
pthread_mutex_t write_mutex;
pthread_mutex_t timestamp_mutex;
pthread_cond_t next_timestamp;

int ring_buffer_init(so_ring_buffer_t *ring, size_t cap)
{
	pthread_mutex_init(&ring->rb_mutex, NULL);
	pthread_cond_init(&ring->prodOk, NULL);
	pthread_cond_init(&ring->consOk, NULL);
	ring->cap = cap;
	ring->data = (char *)malloc(cap);
	ring->write_pos = 0;
	ring->len = 0;
	ring->read_pos = 0;
	ring->connected = 1;
	ring->first = 0;
	ring->last = 0;
	ring->timestamps = (unsigned long *)malloc(5001 * sizeof(unsigned long));
	ring->tcap = 5001;
	return 1;
}

ssize_t ring_buffer_enqueue(so_ring_buffer_t *ring, void *data, size_t size)
{
	pthread_mutex_lock(&ring->rb_mutex);
	int i = 0;
	char *readable_data = (char *)data;

	while (size > ring->cap - ring->len)
		pthread_cond_wait(&ring->consOk, &ring->rb_mutex);
	ring->len += size;
	while (size > 0) {
		ring->data[ring->write_pos] = readable_data[i];
		i++;
		size--;
		ring->write_pos = (ring->write_pos + 1) % ring->cap;
	}
	pthread_mutex_unlock(&ring->rb_mutex);
	pthread_cond_broadcast(&ring->prodOk);
	struct so_packet_t *pkt = (struct so_packet_t *)data;

	pthread_mutex_lock(&timestamp_mutex);
	ring->timestamps[ring->last] = pkt->hdr.timestamp;
	ring->last += 1;
	if (ring->last >= ring->tcap) {
		ring->tcap *= 2;
		ring->timestamps = (unsigned long *)realloc(ring->timestamps, ring->tcap * sizeof(unsigned long));
	}
	pthread_cond_broadcast(&next_timestamp);
	pthread_mutex_unlock(&timestamp_mutex);
	return 0;
}

ssize_t ring_buffer_dequeue(so_ring_buffer_t *ring, void *data, size_t size)
{
	pthread_mutex_lock(&ring->rb_mutex);
	int i = 0;
	char *readable_data = (char *)data;

	while (ring->len < size && ring->connected)
		pthread_cond_wait(&ring->prodOk, &ring->rb_mutex);
	if (ring->len < size) {
		pthread_mutex_unlock(&ring->rb_mutex);
		return 1;
	}
	ring->len -= size;
	while (size > 0) {
		readable_data[i] = ring->data[ring->read_pos];
		i++;
		size--;
		ring->read_pos = (ring->read_pos + 1) % ring->cap;
	}
	pthread_mutex_unlock(&ring->rb_mutex);
	pthread_cond_broadcast(&ring->consOk);
	return 0;
}

void ring_buffer_destroy(so_ring_buffer_t *ring)
{
	free(ring->data);
	pthread_mutex_destroy(&ring->rb_mutex);
	pthread_cond_destroy(&ring->prodOk);
	pthread_cond_destroy(&ring->consOk);
	free(ring->timestamps);
}

void ring_buffer_stop(so_ring_buffer_t *ring)
{
	ring->connected = 0;
	pthread_cond_broadcast(&ring->prodOk);
}
