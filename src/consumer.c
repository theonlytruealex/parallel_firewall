// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include "consumer.h"
#include "ring_buffer.h"
#include "packet.h"
#include "utils.h"

void *consumer_thread(void *ctx_v)
{
	so_consumer_ctx_t *ctx = (so_consumer_ctx_t *)ctx_v;
	int out_fd = open(ctx->out_filename, O_RDWR|O_CREAT|O_APPEND, 0666);
	char buffer[PKT_SZ], out_buf[PKT_SZ];
	
	while (ctx->producer_rb->connected || ctx->producer_rb->len > 0) {
		ring_buffer_dequeue(ctx->producer_rb, buffer, PKT_SZ);
		struct so_packet_t *pkt = (struct so_packet_t *)buffer;
		int action = process_packet(pkt);
		unsigned long hash = packet_hash(pkt);
		unsigned long timestamp = pkt->hdr.timestamp;
		int len = snprintf(out_buf, 256, "%s %016lx %lu\n",
				RES_TO_STR(action), hash, timestamp);
		// to do add mutex
		write(out_fd, out_buf, len);
	}
	close(out_fd);
	return NULL;
}

int create_consumers(pthread_t *tids,
					 int num_consumers,
					 struct so_ring_buffer_t *rb,
					 const char *out_filename)
{
	so_consumer_ctx_t *consumer = (so_consumer_ctx_t *)malloc(num_consumers * sizeof(so_consumer_ctx_t));
	for (int i = 0; i < num_consumers; i++) {
		consumer[i].producer_rb = rb;
		consumer[i].out_filename = out_filename;
		pthread_create(&tids[i], NULL, consumer_thread, (void *)&consumer[i]);
	}
	
	return 1;
}
