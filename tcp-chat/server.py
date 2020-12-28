import threading
import socket

# Connection Data
host = '127.0.0.1'  # local host
port = 7777

# Starting Server
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)      #af_inet - доступные для подключения, стрим - из сокета можно поток сделать - tcp-protocol
server.bind((host, port))
server.listen()

# Lists For Clients and Their Nicknames
clients = []
nicknames = []


# Sending Messages To All Connected Clients
def broadcast(message):
    for client in clients:
        client.send(message)


# Handling Messages From Clients
def handle(client):
    while True:
        try:
            # Broadcasting Messages
            message = client.recv(1024) #receive mesege
            broadcast(message)          #to broadcast
        except:
            # Removing And Closing Clients
            index = clients.index(client)
            clients.remove(client)
            client.close()
            nickname = nicknames[index]
            broadcast('{} left!'.format(nickname).encode('ascii'))
            nicknames.remove(nickname)
            break


# Receiving / Listening Function
def receive():
    while True:
        # Accept Connection
        client, address = server.accept()                       #sleep,wait 
        print("Connected with {}".format(str(address)))

        # Request Nickname
        client.send('NICK'.encode('ascii'))                     #запрос ника клиента
        nickname = client.recv(10240).decode('ascii')           #client return nick & decode it

        # Store Nickname and Client
        nicknames.append(nickname)                              #save nick at list
        clients.append(client)

        # Print And Broadcast Nickname
        print("Nickname is {}".format(nickname))                #message to server
        broadcast("{} joined!".format(nickname).encode('ascii'))
        client.send('Connected to server!'.encode('ascii'))     #message for cl

        # Start Handling Thread For Client - start thread
        thread = threading.Thread(target=handle, args=(client,))
        thread.start()


print('Server is listening...')
receive()       #start func

