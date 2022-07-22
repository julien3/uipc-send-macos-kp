# uipc-send-macos-kp
On macOS 12.5 and earlier with a mac M1, a kernel panic can be triggered when using unix domain sockets. The error message is: **uipc_send connected but no connection?**

## What is the cause?
The kernel panic occurs when calling send (https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/send.2.html) on a Unix domain socket while the remote end has closed the socket. This is likely a race condition and only happens on arm64 (M1).

The code (client.cpp and server.cpp) is a "minimal" code piece to help reproduce the issue. This can surely be made even shorter but the purpose is to trigger the issue with basic operations.

## How to reproduce?
1. Open a terminal then
```
clang++ --std=c++17 -stdlib=libc++ server.cpp -o server
./server
```

2. Open another terminal then
```
clang++ --std=c++17 -stdlib=libc++ client.cpp -o client
./client
```

Then wait until the kernel panic occurs with error message "uipc_send connected but no connection?". 
As this is a race condition, it can take a few seconds up to a few hours to get triggered with this code.

## How does the repro code works?
1) server process listens
2) client process connects to server
3) server accept the connection and starts a thread
4) client sends a string (via send)
5) server receives the string (recv) and displays it
6) server sends a string to client while client is closing socket (client recv call is commented specifically)
7) client repeats step 2, 4 and 6 until ctrl+c or kernel panic

## What is expected?
send should always return an error on server process end and no kernel panic should be triggered at any time.

This code triggers a kernel panic on macOS 12.4 and macOS 12.5 (21G72) with an M1 mac (arm64).
I can't reproduce the issue with Intel macs (x86).
