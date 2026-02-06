# COSC 448 - Directed Studies in Computer Science

to run the server: `gcc -Iinclude server/server.c -o udp_server && ./udp_server`
to run the client: `gcc -Iinclude client/client.c -o client/udp_client && ./client/udp_client`

## The Objective

We are researching is a possible alternative to the standard congestion control algorithms like TCP Tahoe and TCP Reno. Because networks can be so unstable in terms of their reliability (e.g., it's reliable one day, but not so much the next could be due to several external factors like number of devices connected, amount of data flowing, etc.), we're going to try to train an LSTM using network traffic data in an attempt to predict the network's reliability at a given time. The goal is to determine if we can dynamically set things like the congestion window size depending on the model's output given specific inputs, as opposed to the fixed window sizes that current congestion control algorithms use.

## Current Goal - Build TCP on top of UDP

The current goal is to be able to send TCP headers over UDP so that we can have more control over how we send data via TCP without needing to write complex kernel-level logic. This will also enable a persistent connection between two users over UDP. To do this, we are introducing a new `UTCP` socket. A TCP connection is maintained by the kernel via Transmission Control Blocks (TCBs). A TCB contains info necessary A client and a server will both maintain their own data structure that stores TCBs; each active connection (i.e., each socket) has a corresponding TCB.  the TCB contains info like the source port and IP, destination port and IP, stuff for sequence numbers, stuff for ack numbers, and more.

The reason we need to implement TCP ourselves is that the congestion control algos are built directly into the implementations (OS, kernel, C library? not sure), and we need to get around that. To do this, we'll send the packets over UDP which doesn't have any congestion control (it's connectionless and unreliable). This way, when we receive the UDP datagrams, we can grab the TCP data within them and handle the congestion ourselves. There _are_ some lower-level things that we have to manage, such as the TCBs. Sam and I had 2 different approaches to this, but mine ended up being wrong b/c it strayed from TCP too much (I kinda forgot we had to keep the functionality "as close to TCP as possible") so I'll just explain Sam's since that's what we're going with. 

On top of the standard UDP socket (which in C/Unix is just a file descriptor to a socket managed by the kernel) connections are managed by a `UTCP` socket too. To explain this it'll be easier to explain what the UDP headers contain, and what the TCP headers within the UDP datagram's payload contain. 

A UDP header contains (among other things) the destination port and the destination IP. Our UDP headers contain this, as per usual.

However, the TCP header inside the UDP payload contains a UTCP socket, which is just some number we've hardcoded (for now), in the source port and the destination port.

We have a TCB struct which stores a source and destination UTCP port, a src and dest IP addr, the real src UDP port, and a few extra things. The client and server each manage their own array of TCB structs for their connections. This might make more sense with an example. 

For simplicity, let's assume the client and server are running on the same localhost 127.0.0.1.

Client:
Source IP: 127.0.0.1
Source UTCP Port: 1234
Source (UDP) Port: 4567

Server:
Source IP: 127.0.0.1
Source UTCP Port: 9876
Source (UDP) Port: 4444

So the kernel sends data with this stuff over UDP, and when the data arrives somewhere, we have our own logic to demux the data to the correct destination using the UTCP port.