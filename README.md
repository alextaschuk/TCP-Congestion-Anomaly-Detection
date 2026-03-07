# COSC 448 - Directed Studies in Computer Science

## Contents

1. **[Prerequisites](#prerequisites)**
2. **[Cloning the Repository](#cloning-the-repository)**
3. **[How to run the client & server](#how-to-run-the-client--server)**
4. **[Logging](#logging)**
5. **[The Objective](#the-objective)**
6. **[Implementing UDP over TCP (UTCP)](#implementing-udp-over-tcp-utcp)**

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

2. Build and compile the programs

```bash
$ cmake .. && make
```

3. Start the server 
```bash
$ ./server_app
```

4. Start the client (in a separate terminal instance)

```bash
$ ./client_app
``` 

## Logging

The server's logs will be written to `build/log/server/server.log`, and the client's will be written to `build/log/client/client.log`.

### Example output from the client

```bash
00:03:1772871211 [SERVER] [INFO] main_thread: [send_dgram] Sending datagram to UTCP port [49152], UDP port [5555]...
00:03:1772871211 [SERVER] [INFO] main_thread: [send_dgram] Segment that was sent:

[OUTGOING PACKET] >>>
--------------------Header-------------------
	Src UTCP Port    : 332
	Dest UTCP Port   : 49152
	Sequence Number  : 501
	Ack Number       : 101
	Flags            : [ ACK PSH ]
	Window           : 65534
	Size of segment  : 70
	Size of payload  : 38
	Options			 :
		- NOP (Padding)
		- NOP (Padding)
		- Timestamps : TSval = 655826839 ms, TSecr = 655826839 ms
	Payload Data	 : This is a test payload from the server
--------------------------------------------

00:03:1772871211 [SERVER] [INFO] main_thread: [send_dgram] REXMT timer counting down from 2 ticks (1000 ms)
00:03:1772871211 [SERVER] [INFO] listen_thread: [rcv_dgram] Received a packet from 127.0.0.1:5555
00:03:1772871211 [SERVER] [INFO] listen_thread: [rcv_dgram] Received segment:

<<< [INCOMING PACKET]
--------------------Header-------------------
	Src UTCP Port    : 49152
	Dest UTCP Port   : 332
	Sequence Number  : 101
	Ack Number       : 539
	Flags            : [ ACK ]
	Window           : 65496
	Size of segment  : 32
	Size of payload  : 0
	Options			 :
		- NOP (Padding)
		- NOP (Padding)
		- Timestamps : TSval = 655826840 ms, TSecr = 655826839 ms
--------------------------------------------
```

## Background

 Networks can be quite unreliable due to a wide variety of factors that are often not visible or in our control. As such, congestion control algorithms like TCP Tahoe and RENO were developed to help avoid network congestion[^1]. These algorithms were designed to be very broad so that they can avoid congestion at any given moment. Given this, there are two goals of this directed study. The first is to gain an stronger understanding of how TCP and its congestion control algorithms are implemtented (specifically, in C). The second is to research the potential of using LSTMs to identify anamolous network patterns.
 
 The idea behind the second goal is that if an LSTM can reliably detect anomolous patterns, rather than waiting for 3 duplicate ACKs or a timeout to occur, a sender can dynamically update their congestion window according to the receiving network's current reliability. That is, maybe it'd be possible to identify that at some moment in time the ssthresh only needs to drop to 75% of the cwnd at a given time, but at another moment it is better to drop it to 25% of the cwnd.
- This is beyond the scope of this research. We only wish to determine the viability of anomolous network pattern detection via an LSTM, but I thought that the context might be helpful.

## Implementing UDP over TCP (UTCP)

Major operating systems implement transport-layer protocols at the kernel level[^1], and safely modifying and/or editing them is very complicated. Typically, an application accesses this layer to send and receive data via syscalls that use socket file descriptors through Berkley sockets (i.e., calls to `bind()`, `connect()`, and `send()`) For the research, we need to be in control of how an application sends data over TCP. The approach to this has been dubbed by Sam as "UTCP".

In essence, UTCP involes sending data between two peers over UDP using Berkley sockets. The payload of a UDP datagram contains a TCP segment, so it is technically demuxed twice: first to deliver the datagram to the correct UDP socket—this is handled by the kernal—then the TCP segment is demuxed to deliver the payload to the correct UTCP socket.

There are many parallels between kernel implementation and our more simple UTCP version, such as `utcp_bind()`, which binds a UTCP port to a TCB, or the RX & TX buffers that live in a TCB.

### The TCB
To manage the UTCP connections, a custom Transmission Control Block (`TCB`) is used. A TCB contains all of the key information required to maintain a connection between two peers, including:
- A four tuple:
    - source UTCP port
    - source IP address
    - destination UTCP port
    - destination IP address  (it also stores the source UDP and UTCP file descriptors)

- Variables for:
    - Data-order (e.g., sequence numbers)
    - RTT calculations
    - Congestion control
    - Thread synchronization

- A TX (transmit) buffer
- An RX (receive) buffer.
- Some peer-specific information (e.g., a server's TCB contains a SYN queue and an Accept queue)
- And more...

The client and server both maintain a global TCB lookup table that contains the TCB for the listen socket and all TCBs that are either in the process of establishing a connection or have already established one. Because TCP is a full-duplex connection, each connection requires two TCBs: one managed by the client and one by the server. 

### The RX and TX Buffers

The client and server will manage an receive and transmit buffer for every connection. When an application wishes to send to a peer, they can add their data to the TX buffer via `utcp_send()` The client/server then manages sending that data out to the receiver's RX buffer. When the client/server receives data, they place the data into the RX buffer. An application can then pop data from the RX buffer via `utcp_rcv()`.

****

This program has two main parts: `server.c` and `client.c`.

### The Server

The server runs on three threads: a listen thread, a ticker thread, and a main thread.

The listen thread uses a dedicated TCB to listen for incoming connection requests. When a server application is ready to receive connection requests, it calls `utcp_listen()`. Under the hood, this function prepares to read/handle incoming connection requests that the listen thread passes into the SYN and accept queues. Together, `utcp_listen()` and the listen thread create our version of the `listen()` syscall.

The server and client both have a ticker thread. For congestion control, we need to maintain a retransmission timer[^2] for packet timeouts. This thread wakes up every 500ms to decrement a timer. If the timer reaches zero, this thread will also handle the timeout.

The main thread serves the same purpose for the server and the client: it pretends to be an application. In the future, the code to imitate a server application will be cleaned up and moved to its own `utcp_server_app.c` file.

### The Client

The client also runs on three threads: a receive thread, a ticker thread, and a main thread.

The receive thread's job is the exact opposite from the server's listen thread. When a client is ready to connect to a server, it calls `utcp_connect()`. Under the hood, this function is setting up a TCB for the client application and initiating a three-way handshake with a server application. Together, `utcp_connect()` and the listen thread create our version of the `connect()` syscall.

*Note: As of now, the client only receives data from the server, and the server only sends data to the client, hence the server not having a receive thread, and the client not having a listen thread.*




[^1]: For example, as of version [2.6.19](https://github.com/torvalds/linux/commit/597811ec167fa01c926a0957a91d9e39baa30e64), the Linux kernel uses CUBIC (with Reno as a fallback).

[^2]: As of now, _only_ the retransmission timer is implemented.