Alfred Wu


CSE 533 Homework 2 README


The instructions to use the program are as specified in the homework assignment:
* Run make to build executables bin/fclient and bin/fserver.
* After the client starts there will be a prompt for user input in the form of a >.
* Type list to request a file list from the server.
* Type download example.txt to download a file called example.txt.
* Type download example.txt > saved.txt to download a file called example.txt and save it locally in a file called saved.txt.
* Type quit to exit the client.


Notes
* My program assumes that the working directory of fclient and fserver is /HW2 (e.g. one level above them). So it assumes that client.in, server.in, and the /testfiles folder are located in HW2. In addition, the file output by download FILE > FILENAME will be placed in HW2 and not /bin.
* I assume that the minimum possible value of ssthresh is 1.
* As a file or file list is being transferred, blue/green text output corresponds to status messages about the reliable tranmission process (sending packets, ACKing, slow start, etc.) and black text output corresponds to the actual data.
* If the sender is unable to send packets because the receive window has been advertised as being full, after some time it will send a probe packet to the receiver to discover the current status of the receiver window to see if any space has opened up.
* I use a floating point type for the cwnd variable because during congestion avoidance it increases linearly by cwnd + 1 / cwnd per ACK.
* The mechanism I use to signal the end of data is for the sender to transmit to the receiver an empty packet with an end flag enabled.
* When you git pull there will be fclient and fserver executables present one level above /bin in /HW2, please ignore those.