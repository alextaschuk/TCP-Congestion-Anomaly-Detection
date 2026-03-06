/**
 * Configuration for logging via the zlog library
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <zlog.h>

/**
 * If a `.c` file doesn't define a category before including the zlog header,
 * default to `UTCP` so that the compiler doesn't complain. 
 */
#ifndef LOG_CATEGORY
#define LOG_CATEGORY "utcp"
#endif

/**
 * Wrapper macros for zlog.
 * 
 * @note We use ##__VA_ARGS__ to allow for standard printf-style formatting 
 * (e.g., LOG_DEBUG("Value: %d", my_var); )
 */


/**
 * Log a debug message.
 * @param cat The thread logging the message.
 */
#define LOG_DEBUG(fmt, ...) \
    zlog_debug(zlog_get_category(current_thread_cat), fmt, ##__VA_ARGS__)


/**
 * Log an info message.
 */
#define LOG_INFO(fmt, ...) \
    zlog_info(zlog_get_category(current_thread_cat), fmt, ##__VA_ARGS__)

/**
 * Log a warning.
 */
#define LOG_WARN(fmt, ...) \
    zlog_warn(zlog_get_category(current_thread_cat), fmt, ##__VA_ARGS__)


/**
 * Log an error.
 */
#define LOG_ERROR(fmt, ...) \
    zlog_error(zlog_get_category(current_thread_cat), fmt, ##__VA_ARGS__)

    
/**
 * Log a fatal (i.e., severe) error.
 */
#define LOG_FATAL(fmt, ...) \
    zlog_fatal(zlog_get_category(current_thread_cat), fmt, ##__VA_ARGS__)

/**
 * Initialize the zlog logger with `zlog.conf`.
 * 
 * @param *conf_path A filepath to either `zlog_client.conf` or
 * `zlog_server.conf`. Since the server and client's logs will be
 * split, we need to know who is initializing the logger when
 * they call this function.
 * 
 * @returns `0` on success, `-1` on failure.
 */
int init_zlog(char conf_path[]);


#endif // LOGGER_H
