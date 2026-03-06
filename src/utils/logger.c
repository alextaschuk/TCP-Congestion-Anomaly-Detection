#include <utils/logger.h>


int init_zlog(char conf_path[])
{
    /* initialize zlog w/ zlog.conf */
    int rc = dzlog_init(conf_path, "UTCP");
    
    if (rc) {
        printf("zlog init failed\n");
        return -1;
    }
    return 0;
}