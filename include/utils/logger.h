/**
 * Configuration for logging via the zlog library
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <tcp/tcb.h>
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

/**
 * @brief Logs one row of LSTM training data to the lstm_data CSV.
 *
 * Call sites (always called with the TCB lock held):
 * 
 *   1. `handle_data()`: Called after `cc_ops_t.ack_received` is called for a valid ACK.
 *      `rtt_us` = the measured RTT that is stored in `tcb->rtt` (0 if no sample for the ACK),
 *      `newly_acked` = new bytes ACKed from the packet
 * 
 *   2. `handle_data()`: Called after `cc_ops_t.duplicate_ack` is called for a dupe ACK.
 *      `rtt_us` = 0, `newly_acked` = 0, `is_dup_ack` = true
 * 
 *   3. `handle_rexmt_timeout()`: Called before snd_nxt is rolled back and `cc_ops_t.timeout`
 *      is called. `rtt_us` = 0, `newly_acked` = 0, `is_timeout` = true. `flight_size` is read
 *      from the TCB before the rollback by the logger itself.
 *
 * The CSV has the following columns (the file itself does not have any headers): `timestamp_us`,
 * `rtt_us`, `srtt_us`, `rttvar_us`, `rto_us`, `min_rtt_us`, `queue_delay_us`, `rtt_delta_us`,
 * `rtt_accel_us`, `rto_delta_us`, `cwnd`, `ssthresh`, `snd_wnd`, `flight_size`, `newly_acked`,
 * `inter_ack_us`, `ca_state`, `t_dupacks`, `t_rxtshift`, `is_dup_ack`, `is_timeout`
 */
void log_lstm_event(struct tcb_t *tcb_t, uint32_t rtt_us, uint32_t newly_acked, bool is_dup_ack, bool is_timeout);


#endif // LOGGER_H
