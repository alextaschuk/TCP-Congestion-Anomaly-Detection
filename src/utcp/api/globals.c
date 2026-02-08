#include <utcp/api/globals.h>

#include <stdio.h>

static api_t global;

api_t *api_instance(void)
{
    static int initialized = 0;
    if (!initialized)
    {
        // client info
        global.client_port = 5555;
        global.client_utcp_port = 776; 
        global.client_ip = "127.0.0.1";
        // server info
        global.server_port = 4567;
        global.server_utcp_port = 332;
        global.server_ip = "127.0.0.1";

        for (int i = 0; i < MAX_UTCP_SOCKETS; i++)
            global.tcp_lookup[i] = NULL;
        
        initialized = 1;
    }
    return &global;
}
