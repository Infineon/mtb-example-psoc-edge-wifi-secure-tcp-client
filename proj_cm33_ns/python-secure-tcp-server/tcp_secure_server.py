#******************************************************************************
# File Name:   tcp_secure_server_ipv6.py
#
# Description: A simple secure TCP server for demonstrating TCP usage.
# 
#*******************************************************************************
#* (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
#* Technologies AG. All rights reserved.
#* This software, associated documentation and materials ("Software") is
#* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
#* and is protected by and subject to worldwide patent protection, worldwide
#* copyright laws, and international treaty provisions. Therefore, you may use
#* this Software only as provided in the license agreement accompanying the
#* software package from which you obtained this Software. If no license
#* agreement applies, then any use, reproduction, modification, translation, or
#* compilation of this Software is prohibited without the express written
#* permission of Infineon.
#* 
#* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
#* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
#* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
#* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
#* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
#* Infineon reserves the right to make changes to the Software without notice.
#* You are responsible for properly designing, programming, and testing the
#* functionality and safety of your intended application of the Software, as
#* well as complying with any legal requirements related to its use. Infineon
#* does not guarantee that the Software will be free from intrusion, data theft
#* or loss, or other breaches ("Security Breaches"), and Infineon shall have
#* no liability arising out of any Security Breaches. Unless otherwise
#* explicitly approved by Infineon, the Software may not be used in any
#* application where a failure of the Product or any consequences of the use
#* thereof can reasonably be expected to result in personal injury.
#*******************************************************************************

#!/usr/bin/python

import socket, ssl
import optparse
import time
import sys

host = ''       # Symbolic name meaning the local host 
port = 50007    # Arbitrary non-privileged port

# If argument passed is ipv6, use IPv6 addressing mode.
if ( len(sys.argv) > 1 and sys.argv[1] == "ipv6" ):    
    print("=============================================================================")
    print("TCP Secure Server (IPv6 addressing mode)")
    print("=============================================================================")
    s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

# If any argument other than ipv6 is passed, use  IPv4 addessing mode.
else :
    print("=============================================================================")
    print("TCP Secure Server (IPv4 addressing mode)")
    print("=============================================================================")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(certfile="server.crt", keyfile="server.key")
context.load_verify_locations(cafile="root_ca.crt")

try:
    s.bind((host, port))
    s.listen(1)
except socket.error as msg:
    print("ERROR: ", msg)
    s.close()
    s = None

if s is None:
    sys.exit(1)

while 1:
    print("Listening on port: %d"%(port))
    data_len = 0
    try:
        conn, addr = s.accept()
        connstream = context.wrap_socket(conn, server_side=True)
    except KeyboardInterrupt:
        print("Closing Connection")
        s.close()
        s = None
        sys.exit(1)

    print('Incoming connection accepted: ', addr)

    try:
        while 1:
            data = input("Enter your option: '1' to turn ON LED, 0 to turn"\
                         " OFF LED and Press the 'Enter' key: ")
            if(data == ""):
                print("No option entered!")
            else:
                connstream.write(data.encode())
                data = connstream.read(4096)
                if not data: break
                print("Acknowledgement from TCP Client:", data.decode('utf-8'))
                print("")
            
    except KeyboardInterrupt:
        conn.close()
        s.close()
        s = None
        print("\nConnection Closed")
        sys.exit(1)

# [] END OF FILE
 
