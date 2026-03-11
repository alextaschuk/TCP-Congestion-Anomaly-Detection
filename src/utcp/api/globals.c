#include <utcp/api/globals.h>

#include <stdio.h>
#include <stdlib.h>

#include <utils/logger.h>

static api_t global;

static int initialized = 0;

api_t *api_instance(void)
{
    if (!initialized)
    {
        LOG_INFO("[api_instance] Initializing the global struct");
        // client info
        global.client_port = 5555;
        global.client_utcp_port = 776; 
        //global.client_ip = "127.0.0.1";
        global.client_ip = "40.82.162.155";
        // server info
        global.server_port = 4567;
        global.server_utcp_port = 332;
        global.server_ip = "127.0.0.1";

        pthread_mutex_init(&global.lookup_lock, NULL);
        
        //LOG_INFO("[api_instance] Locking the lookup table to initialize it with NULL");
        pthread_mutex_lock(&global.lookup_lock);
        
        for (int i = 0; i < MAX_CONNECTIONS; i++)
            global.tcb_lookup[i] = NULL;
        
        //LOG_INFO("[api_instance] Finished lookup table init. Unlocking the lookup table.");
        pthread_mutex_unlock(&global.lookup_lock);

        initialized = 1;
    }

    return &global;
}
