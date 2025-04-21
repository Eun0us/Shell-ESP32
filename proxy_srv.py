import socket
import threading
import time

clients = {}
lock = threading.Lock()

def handle_client(client_socket, address):
    client_id = f"{address[0]}:{address[1]}"
    print(f"[+] Nouveau client connecté : {client_id}")

    with lock:
        clients[client_id] = client_socket

    try:
        while True:
            data = client_socket.recv(4096)
            if not data:
                break
            print(f"\n[{client_id}] {data.decode(errors='ignore')}")
    except Exception as e:
        print(f"[!] Erreur avec {client_id} : {e}")
    finally:
        with lock:
            if client_id in clients:
                del clients[client_id]
        client_socket.close()
        print(f"[-] Client déconnecté : {client_id}")

def accept_connections(server_socket):
    while True:
        client_socket, addr = server_socket.accept()
        threading.Thread(target=handle_client, args=(client_socket, addr), daemon=True).start()

def command_sender():
    last_clients_state = []

    while True:
        time.sleep(0.5)
        with lock:
            client_list = list(clients.keys())

        if client_list != last_clients_state:
            print("\nClients connectés :")
            if not client_list:
                print("  Aucun client connecté.")
            else:
                for i, client_id in enumerate(client_list):
                    print(f"  [{i}] {client_id}")
            last_clients_state = client_list

        if client_list:
            try:
                choice = input("\nChoix client (numéro ou ENTER pour ignorer) : ").strip()
                if choice == "":
                    continue

                target = int(choice)
                client_id = client_list[target]
                msg = input(f"Commande à envoyer à {client_id} : ")
                with lock:
                    clients[client_id].sendall(msg.encode())
            except (ValueError, IndexError):
                print("[!] Numéro invalide.")
            except Exception as e:
                print(f"[Erreur] {e}")

def start_server(host="0.0.0.0", port=1234):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((host, port))
    server.listen(5)
    print(f"[Serveur] En écoute sur {host}:{port}")

    threading.Thread(target=accept_connections, args=(server,), daemon=True).start()
    command_sender()

if __name__ == "__main__":
    start_server()
