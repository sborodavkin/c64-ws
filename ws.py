import os
import socket

HOST, PORT = '', 9091


dev = os.open("/dev/pts/24", os.O_RDWR)

listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listen_socket.bind((HOST, PORT))
listen_socket.listen(1)
print 'Serving HTTP on port %s ...' % PORT
while True:
    client_connection, client_address = listen_socket.accept()
    request = client_connection.recv(1024)
    print ">>>" + request + "<<<"
    
    if 'stop.html' in request:
      os.close(dev)
      print 'Goodbye.'
      break
    
    os.write(dev, request)
    
    response = bytearray()

    while ':END:' not in response:
      response.extend(bytearray(os.read(dev, 4096)))
    response = response[0:-5]
    print response  # ' '.join(hex(x) for x in response)
  
    client_connection.send(response)
    client_connection.close()

    response = bytearray()
