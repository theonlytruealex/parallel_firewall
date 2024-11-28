// SPDX-License-Identifier: BSD-3-Clause

#include "ring_buffer.h"


pthread_mutex_t rb_mutex;
pthread_cond_t prodOk, consOk;
pthread_mutex_t write_mutex;

int ring_buffer_init(so_ring_buffer_t *ring, size_t cap)
{
	pthread_mutex_init(&rb_mutex, NULL);
	pthread_cond_init(&prodOk, NULL);
	pthread_cond_init(&consOk, NULL);
	ring->cap = cap;
	ring->data = (char *)malloc(cap);
	ring->write_pos = 0;
	ring->len = 0;
	ring->read_pos = 0;
	ring->connected = 1;

	return 1;
}

ssize_t ring_buffer_enqueue(so_ring_buffer_t *ring, void *data, size_t size)
{
	pthread_mutex_lock(&rb_mutex);
	if (size > ring->cap) {
		pthread_mutex_unlock(&rb_mutex);
		return 1;
	}
	int i = 0;
	char *readable_data = (char *)data;

	while (size > ring->cap - ring->len)
		pthread_cond_wait(&consOk, &rb_mutex);
	ring->len += size;
	while (size > 0) {
		ring->data[ring->write_pos] = readable_data[i];
		i++;
		size--;
		ring->write_pos = (ring->write_pos + 1) % ring->cap;
	}
	pthread_mutex_unlock(&rb_mutex);
	pthread_cond_broadcast(&prodOk);
	return 0;
}

ssize_t ring_buffer_dequeue(so_ring_buffer_t *ring, void *data, size_t size)
{
	pthread_mutex_lock(&rb_mutex);
	if (size > ring->cap) {
		pthread_mutex_unlock(&rb_mutex);
		return 1;
	}
	int i = 0;
	char *readable_data = (char *)data;

	while (ring->len < size && ring->connected)
		pthread_cond_wait(&prodOk, &rb_mutex);
	if (ring->len < size) {
		pthread_mutex_unlock(&rb_mutex);
		return 1;
	}
	ring->len -= size;
	while (size > 0) {
		readable_data[i] = ring->data[ring->read_pos];
		i++;
		size--;
		ring->read_pos = (ring->read_pos + 1) % ring->cap;
	}
	pthread_mutex_unlock(&rb_mutex);
	pthread_cond_broadcast(&consOk);
	return 0;
}

void ring_buffer_destroy(so_ring_buffer_t *ring)
{
	free(ring->data);
	pthread_mutex_destroy(&rb_mutex);
	pthread_cond_destroy(&prodOk);
	pthread_cond_destroy(&consOk);
}

void ring_buffer_stop(so_ring_buffer_t *ring)
{
	ring->connected = 0;
	pthread_cond_broadcast(&prodOk);
}
