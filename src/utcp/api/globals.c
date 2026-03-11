#include <utcp/api/globals.h>

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <utils/logger.h>

static api_t global;

static int initialized = 0;

api_t *api_instance(void)
{
    if (!initialized)
    {
        LOG_INFO("[api_instance] Initializing the global struct");
        
        /* Client connection info */
        global.client_udp_port = 5555;
        global.client = (struct sockaddr_in) {
            .sin_family = AF_INET,
            .sin_port = htons(776) // UTCP port
        };

        if (inet_pton(AF_INET, "40.82.162.155", &global.client.sin_addr) <= 0)
            LOG_ERROR("[api_instance] Invalid client IPv4 address.");
        
        //if (inet_pton(AF_INET, "127.0.0.1", &global.client.sin_addr) <= 0)
            //LOG_ERROR("[api_instance] Invalid client IPv4 address.");

        /* Server connection info */
        global.server_udp_port = 1970;
        global.server = (struct sockaddr_in){
            .sin_family = AF_INET,
            .sin_port = htons(332), // UTCP port
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };

        //if (inet_pton(AF_INET, "127.0.0.1", &global.server.sin_addr) <= 0)
        //    LOG_ERROR("[api_instance] Invalid server IPv4 address.");

        /* TCB lookup table config */
        pthread_mutex_init(&global.lookup_lock, NULL);

        pthread_mutex_lock(&global.lookup_lock);
        for (int i = 0; i < MAX_CONNECTIONS; i++)
            global.tcb_lookup[i] = NULL;
        pthread_mutex_unlock(&global.lookup_lock);

        initialized = 1;
    }

    return &global;
}
