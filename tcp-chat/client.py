import socket
import threading

# Choosing Nickname
nickname = input("Choose your nickname: ")

# Connecting To Server
client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(('127.0.0.1', 7777))


# Listening to Server and Sending Nickname
def receive():
    global stop_thread
    stop_thread = False
    while True:
        if stop_thread:
            break
        try:
            # Receive Message From Server
            # If 'NICK' Send Nickname
            message = client.recv(1024).decode('ascii')
            if message == 'NICK':
                client.send(nickname.encode('ascii'))
            else:
                print(message)
        except:
            # Close Connection When Error
            print("An error occurred!")
            client.close()
            stop_thread = True
            break


# Sending Messages To Server
def write():
    while True:
        if stop_thread:
            break
        message = '{}: {}'.format(nickname, input(''))
        client.send(message.encode('ascii'))


# Starting Thread for Listening
receive_thread = threading.Thread(target=receive)
receive_thread.start()

# Starting Thread for Writing
write_thread = threading.Thread(target=write)
write_thread.start()
