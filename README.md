# COSC 448 - Directed Studies in Computer Science

## Contents

1. **[Prerequisites](#prerequisites)**
2. **[Cloning the Repository](#cloning-the-repository)**
3. **[How to run the client & server](#how-to-run-the-client--server)**
4. **[Logging](#logging)**
5. **[Background](#background)**
6. **[Implementing UDP over TCP (UTCP)](#implementing-udp-over-tcp-utcp)**
7. **[Results](#results)**

## Prerequisites

* **CMake** (v3.10 or higher)
* A C11 compatible compiler (Clang or GCC)

*Note: This project uses [**zlog**](https://github.com/HardySimpson/zlog) for thread-safe logging. It is included as a Git Submodule.*

## Cloning the Repository
Because of zlog, you'll need to clone this repository recursively to fetch the library's source code:

```bash
$ git clone --recursive https://github.com/alextaschuk/cosc-448-directed-study
$ cd cosc-448-directed-study
```
- If you cloned the repository without this flag, run `git submodule update --init` to install zlog locally.

## How to run the client & server

1. Create a `/build` directory

```bash
$ mkdir build && cd build
```

2. Build and compile the program, then run the server or client (in separate terminal instances)

```bash
$ bash ../scripts/run_server.sh # run the server

$ bash ../scripts/run_client.sh # run the client
```


## Logging

The server's logs will be written to `build/log/server.log`, and the client's will be written to `build/log/client.log`. Additionally, the sender's 
events like ACKs, timeouts, and triple ACKs will be logged in `build/logs/events.csv` for easy plotting.

### Example output from the client

```bash
1773986363.912 [CLIENT] [DEBUG] main_thread: [send_dgram] snd_nxt advanced by 1400 bytes. New snd_nxt=2801
1773986363.912 [CLIENT] [DEBUG] main_thread: [send_dgram] Advanced snd_max to 2801
1773986363.912 [CLIENT] [INFO] main_thread: Preparing to send 1400 bytes of payload.
1773986363.912 [CLIENT] [INFO] main_thread: 

[OUTGOING PACKET] >>>
--------------------Header-------------------
	Src UTCP Port    : 8292
	Dest UTCP Port   : 332
	Sequence Number  : 2801
	Ack Number       : 1
	Flags            : [ ACK ]
	Window           : 32768
	Size of segment  : 1432
	Size of payload  : 1400
	Options			 :
		- NOP (Padding)
		- NOP (Padding)
		- Timestamps : TSval = 79413666 ms, TSecr = 1114724775 ms
--------------------------------------------

1773986363.912 [CLIENT] [DEBUG] main_thread: [send_dgram] snd_nxt advanced by 1400 bytes. New snd_nxt=4201
1773986363.912 [CLIENT] [DEBUG] main_thread: [send_dgram] Advanced snd_max to 4201
1773986363.912 [CLIENT] [DEBUG] main_thread: [send_dgram] Finished sending data in TX. Burst 3 segments, 4296 total bytes.
1773986363.983 [CLIENT] [INFO] listen_thread: 

<<< [INCOMING PACKET]
--------------------Header-------------------
	Src UTCP Port    : 332
	Dest UTCP Port   : 8292
	Sequence Number  : 1
	Ack Number       : 2801
	Flags            : [ ACK ]
	Window           : 32680
	Size of segment  : 32
	Size of payload  : 0
	Options			 :
		- NOP (Padding)
		- NOP (Padding)
		- Timestamps : TSval = 1114724846 ms, TSecr = 79413666 ms
--------------------------------------------

1773986363.983 [CLIENT] [DEBUG] listen_thread: [calc_rto] RTO 17 ticks clamped up to TCPTV_MIN=20 ticks.
1773986363.983 [CLIENT] [INFO] listen_thread: [handle_data] VALID ACK: Advancing snd_una from 1 to 2801 (ACKed 2800 bytes)
1773986363.983 [CLIENT] [DEBUG] listen_thread: [handle_data] SND_WND UPDATE: tx_head 0 -> 2800, snd_wnd set to 1045760. Waking any blocking app threads.
1773986363.983 [CLIENT] [DEBUG] listen_thread: [reset_timer] REXMT RESET: base_rto=20, shift=0, rxtcur=20 ms
1773986363.983 [CLIENT] [INFO] listen_thread: [handle_data] SLOW START: cwnd 4200 -> 5600
```

## Background

Network reliability has a very non-deterministic behavior due to a wide array of influences, many of which are often not visible or in our control. Two significant factors that contribute to this behavior are the speed at which a peer can send/receive data over the network (hardware limitations) and the speed at which a peer’s application can read/write data to the peer’s send and receive buffer. Congestion control algorithms such as TCP Tahoe, Reno, and NewReno were developed to help prevent and manage network congestion. These algorithms are designed to work under a variety of network conditions in order to avoid and handle congestion at any given moment, and they rely on two main concepts. The first is called flow control, and it helps protect a peer’s application from being sent too much data too quickly. The receiving peer advertises a receive window (rwnd) to inform the sending peer how much data their application can handle receiving. The second concept is called congestion control, and it helps protect a peer’s network by advertising a congestion window (cwnd) to ensure that the sending peer does not send more data than the network can handle. Together, the rwnd and cwnd put a ceiling on how many unacknowledged (unACKed) bytes the sender can have in-flight at once. With this information, peers can minimize the likelihood of exceeding a network’s and/or application’s capacity to receive data.

Given this, there are two goals for this directed study. The first is to gain a stronger understanding of how TCP and its congestion control algorithms are implemented (specifically in C). The second is to explore the potential of using a long short-term memory (LSTM) neural network to detect anomalous network patterns. The idea behind the second goal is that if an LSTM can reliably identify anomalous patterns, a sender could dynamically adjust their congestion window based on the network's current reliability, rather than waiting for 3 duplicate acknowledgments (ACKs) or a timeout. For example, the LSTM might recognize that at some moment in time, the slow start threshold (ssthresh) only needs to drop to 75% of the congestion window (cwnd), whereas at another moment, it could be more effective to drop it to 25% of the cwnd.[^1]


## Implementing UDP over TCP (UTCP)

Major operating systems implement transport-layer protocols at the kernel level[^2], and safely modifying/editing them is very complicated. A program can access the transport-layer to manage connections, send data, and receive data through the Berkley sockets API (or BSD sockets) via functions such as `connect()`, `send()`, and `recv()`. For this research, we needed to be in control of how an application sends and receives data over TCP at the transport layer. The approach to this has been dubbed by Sam as "UTCP". In essence, UTCP involves sending data between two peers over UDP. The payload of a UDP datagram contains a TCP segment, so it is technically demultiplexed (demuxed) twice: first to deliver the datagram to the correct UDP socket—this is handled by the kernel—then the TCP segment is demuxed to deliver the payload to the correct UTCP socket. Since packets are being sent over UDP and we are implementing TCP by ourselves, we need a way to maintain the connection-oriented, reliable service that TCP promises. This means that all TCP-related logic is handled through UTCP rather than the kernel, giving us complete control over the protocol. From an application perspective, data is being sent over UDP.

There are many parallels between standard kernel implementations of TCP and our simpler UTCP version. Every peer maintains a Transmission Control Block (TCB) for a connection that is either in the process of establishing a connection through a three-way handshake (3WHS) or has completed a 3WHS and is in an `ESTABLISHED` state. A TCB contains all the necessary information to maintain a connection between two peers and several key variables and structs necessary for the features of TCP that we have implemented. For instance, like TCP, a UTCP connection is identified by a four-tuple, which is made up of the source UTCP port, source IPv4 address, destination UTCP port, and destination IPv4 address.

Each side of a connection runs three threads: a listen thread, a ticker thread, and an application thread. While UTCP can support multiple simultaneous fully-duplex connections for our research, we are currently working with two peers—one client and one server—where the server sends data to the client. 

### The Listen Thread
The listen thread manages all incoming segments for a peer and demuxes them to the intended receiver. It handles three types of segments. First, it processes a synchronize (SYN) request, which initializes a 3WHS between the sender and receiver. The listen thread creates a new TCB using the four-tuple in the segment’s header, populates it with the initial connection’s values, and adds the TCB to the SYN queue. The SYN queue holds all TCBs that are in the process of establishing a connection. Finally, it sends a SYN-ACK segment to continue the 3WHS. Second, it handles an ACK segment sent in response to a SYN-ACK, which completes the 3WHS. The listen thread then moves the now-established connection’s TCB into the accept queue, where an application can call `utcp_accept()` to get the connection’s UDP socket descriptor and pop the TCB from the accept queue. Third, it demuxes received segments for an established connection to the correct UTCP socket to be handled accordingly, whether that be a segment with a data payload that will be placed into a receive (RX) buffer, or an ACK for data that was sent out.

### The Ticker Thread
The ticker thread’s only purpose is to wake and decrease a retransmission timeout (RTO) timer’s count by one every ten milliseconds. When the timer expires, a handler function is called to perform exponential backoff[^3].

### The Application Thread
The application thread simulates a peer’s application functions. For both the client and the server, the thread binds a UDP port to a socket, then a UTCP port to a socket. The rest of its operations differ slightly between the client and the server after the sockets are bound. For the client, the thread calls `utcp_connect()` to initiate a 3WHS with the server. It then allocates an application receive buffer and prepares to receive data from the server. For the server, the thread calls `utcp_accept()` to accept the client’s incoming connection request. Then, it allocates an application send buffer and begins sending a stream of bytes to the client.

### Features

Several of TCP’s features have been implemented into UTCP, including:
- Ensuring that data is sent as a reliable in-order stream of bytes.
- A full-duplex connection between peers.
- A three-way handshake process to establish a connection between two peers.
- A SYN and accept queue for tracking and managing connections that are in the process of being established.
- A sliding window for flow control.
- Layer 1 (Application ) send and receive buffers, and Layer 2 (Transport) transmit and receive buffers.
- The Window Scale Option in SYN headers to increase the rwnd past the standard 65,535 bytes.
- The Timestamps Option for easy and accurate round-trip time (RTT) calculations.
- Calculating a peer’s retransmission timeout (RTO) value via the Jacobson/Karels Algorithm. The RTO determines how many ticks until a peer’s retransmission timer runs out and needs to handle a packet timeout.
- TPC Tahoe, Reno, and NewReno for congestion control and congestion avoidance. These algorithms are fully modular and can be easily switched out for one another using the global `CC_ALGO` object-like macro.
- An out-of-order buffer to minimize the amount of retransmitted data upon packet loss.

### Functions

All UTCP application-side functions are designed to mirror a BSD socket function:

`utcp_sock()`
- Binds a UDP socket, spins up the listen and ticker threads, initializes a TCB for a new UTCP socket, and returns the UTCP socket descriptor. The new TCB is in the `CLOSED` state. This is equivalent to the BSD `sock()` function.

`utcp_bind(int utcp_fd, sockaddr_in peer)`
- Binds a UTCP port, UDP port, and IPv4 address to the TCB with socket descriptor `utcp_fd`. This is equivalent to the BSD `bind()` function.

`utcp_connect(int utcp_fd, sockaddr_in dest_addr)`
- Creates a new TCB upon receiving a SYN packet, initializes it using the four-tuple in the packet’s header, moves the `SYN-SENT` state, and sends a SYN packet. Equivalent to the BSD `connect()` function.

`utcp_listen(api_t *global, int backlog)`
- Announces that the application is ready to receive connection requests. In this function, the SYN and accept queues are initialized, and the application’s TCB moves from the `CLOSED` state to the LISTEN state. Equivalent to the BSD `listen()` function.

`utcp_accept(api_t *global)`
- Called when the application wants to accept a connection request that is sitting in the accept queue. This function returns the connection’s UTCP socket descriptor. Equivalent to the BSD `accept()` function.

`utcp_send(int utcp_fd, void *buf, size_t payload_len)`
- Called when the application wants to send data to a peer. This function places up to `payload_len` number of bytes into the sending peer’s transmit (TX) buffer and attempts to send the data. Equivalent to the BSD `send()` function.

`utcp_recv(int utcp_fd, void *buf, size_t app_buf_len)`
- Called when the application wants to receive data from a peer. This function reads up to `app_buf_len` number of bytes from the RX buffer and writes them into the application’s receive buffer. Equivalent to the BSD `recv()` function.

## Results

In order to test UTCP, I generated a 1GB text file containing random, printable characters and sent it from a “Standard B2ats v2” virtual machine, which was hosted by Microsoft Azure in the Central Canada region, to my local MacBook Pro (2024 M4 Pro, 24GB RAM) over Wi-Fi. The VM served as a server application, and the MacBook served as a client application. The figures plot four to five events, depending on the congestion algorithm: cwnd, ssthresh, triple ACKs, timeouts, and fast recovery (NewReno only). Per [RFC 5681, section 3.1](https://datatracker.ietf.org/doc/html/rfc5681#section-3.1), the “initial value of ssthresh SHOULD be set arbitrarily high.” As such, the plots have been truncated to the first timeout or triple ACK event that occurs. 

### TCP Tahoe

TCP Tahoe is the simplest congestion control algorithm of the three. In the event of a triple ACK, Tahoe sets the ssthresh to `MAX(50% of bytes in flight, 2 × MSS)`, drops the cwnd to 1 MSS (maximum segment size), and performs a Fast Retransmit by attempting to resend the dropped packet. In the event that the retransmission timer expires, Tahoe sets the ssthresh to `MAX(50% of bytes in flight, 2 × MSS)`, drops the cwnd to 1 MSS (maximum segment size), and resends the oldest unACKed segment.


[^1]: This use case is out of the scope of this research. We only wish to determine the viability of anomalous network pattern detection via an LSTM, but I thought the context might be helpful. 
[^2]: For example, as of version [2.6.19](https://github.com/torvalds/linux/commit/597811ec167fa01c926a0957a91d9e39baa30e64), the Linux kernel uses CUBIC (with Reno as a fallback).
[^3]: See [RFC 6298, Rule 5.5](https://datatracker.ietf.org/doc/html/rfc6298#section-5).