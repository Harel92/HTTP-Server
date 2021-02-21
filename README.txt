README
Name: Harel Madmoni
Id: 203382080

Exercise 3 - HTTP server

The program creates  HTTP server and a threadpool , that can handle a finite number of server entries.


===Description ===

Program files:

server.c
threadpool.c

// compile: gcc server.c -o server -lpthread
// run: ./server <port> <pool-size> <max-number-of-request>

only supports "GET" method.
max number of pool size is 200.
-----------------------------
How to run:
Use the web to connect to the server and ask for files.

Response:

"400 Bad Request" - The request didn't built properly.
"501 not supported"- Method is not "GET".
404 Not Found - File not found.
500 "Internal Server Error" - Error in server.
302 Found - Missing "/" at the end of the requested directory.
403 Forbidden - Cannot access file.(not regular file or no permissions)
200 OK - Proper request.		
------------------
