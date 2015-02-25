# Forza4
Forza4's game implementation using Client/Server architecture. 

The project consists in development of Forza4's game using Client/Server architecture. 
Both the server and the client are mono-process and use I/O multiplexing to handle multiple input simultaneously. 
The game map is represented by a grid of lines 6 and 7 columns.
Each square's grid is identified by a pair consisting of a letter [ag] and a number [1-6].
During a match each player has one of the two symbols of the game: 'X' or 'O'. 
The server stores connected users and ports that users will listen. 
Information's exchange between client and server is through TCP sockets. This information are only control information that are used to implement peer2peer communication. 
The exchange of messages between clients is via UDP socket.
