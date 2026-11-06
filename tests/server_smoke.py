"""Loopback integration test: four authenticated sessions, request framing, privacy and reconnect."""
import json
import os
import select
import socket
import subprocess
import sys
import time

class Client:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=3)
        self.socket.settimeout(3)
        self.buffer = b""
        self.messages = []

    def send(self, line):
        self.socket.sendall((line + "\n").encode("utf-8"))

    def until(self, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            while b"\n" in self.buffer:
                line, self.buffer = self.buffer.split(b"\n", 1)
                value = json.loads(line)
                self.messages.append(value)
                if predicate(value):
                    return value
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            readable, _, _ = select.select([self.socket], [], [], remaining)
            if readable:
                chunk = self.socket.recv(65536)
                if not chunk:
                    raise AssertionError("Server closed the connection")
                self.buffer += chunk
        raise AssertionError("Timed out waiting for a protocol response")

    def close(self):
        self.socket.close()

def main(executable):
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    server = subprocess.Popen(
        [executable, "--port", "0", "--seed", "77"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        creationflags=flags)
    clients = []
    try:
        line = server.stdout.readline().strip()
        assert line.startswith("LISTENING "), line
        port = int(line.rsplit(":", 1)[1])
        states, tokens = [], []
        for i in range(4):
            client = Client(port)
            clients.append(client)
            client.until(lambda x: x["type"] == "hello")
            client.send("JOIN " + ('Alice"中文' if i == 0 else "player" + str(i)))
            welcome = client.until(lambda x: x["type"] == "welcome")
            assert welcome["seat"] == i
            tokens.append(welcome["token"])
        for i, client in enumerate(clients):
            state = client.until(lambda x: x["type"] == "state" and x["phase"] == "shopping")
            assert state["seat"] == i and state["hand"] == []
            assert state["players"][0]["name"] == 'Alice"中文'
            for peer in state["players"]:
                assert peer["coins"] == 6 if peer["seat"] == i else peer["coins"] is None
            states.append(state)

        # A request split over packets and two requests in a single packet.
        clients[0].socket.sendall(b"1 STA")
        clients[0].socket.sendall(b"TE\n2 STATE\n")
        clients[0].until(lambda x: x["type"] == "ack" and x["request"] == 1)
        clients[0].until(lambda x: x["type"] == "ack" and x["request"] == 2)
        clients[0].send("2 STATE")
        clients[0].until(lambda x: x["type"] == "duplicate")
        clients[0].send("3 SCORE 999")
        clients[0].until(lambda x: x["type"] == "error")

        fifth = Client(port)
        clients.append(fifth)
        fifth.until(lambda x: x["type"] == "hello")
        fifth.send("JOIN intruder")
        fifth.until(lambda x: x["type"] == "error")

        ids = [4, 1, 1, 1]
        for i in range(4):
            clients[i].send(f'{ids[i]} DONE {states[i]["shop"]["id"]}')
        playing = []
        for i in range(4):
            state = clients[i].until(lambda x: x["type"] == "state" and x["phase"] == "playing")
            assert len(state["hand"]) == 7 and state["deck"] == 26
            assert all(v["handCount"] == 7 for v in state["players"])
            playing.append(state)

        acting = playing[0]["acting"]
        turn = playing[0]["turn"]
        wrong = (acting + 1) % 4
        ids[wrong] += 1
        clients[wrong].send(f'{ids[wrong]} PLAY {turn} {playing[wrong]["hand"][0]["id"]}')
        clients[wrong].until(lambda x: x["type"] == "error")
        own = next(card for card in playing[acting]["hand"] if not card["joker"])
        ids[acting] += 1
        play_id = ids[acting]
        clients[acting].send(f'{play_id} PLAY {turn} {own["id"]}')
        clients[acting].until(lambda x: x["type"] == "ack" and x["request"] == play_id)
        state = clients[acting].until(
            lambda x: x["type"] == "state" and x["turn"] == turn + 1)
        assert len(state["hand"]) == 6
        assert state["players"][acting]["score"] == 1
        assert state["players"][acting]["coins"] == 7
        assert len(state["lastPlay"]) == 1
        clients[acting].send(f'{play_id} PLAY {turn} {own["id"]}')
        clients[acting].until(lambda x: x["type"] == "duplicate")
        state = clients[acting].until(lambda x: x["type"] == "state" and x["turn"] == turn + 1)
        assert len(state["hand"]) == 6

        replacement = Client(port)
        clients.append(replacement)
        replacement.until(lambda x: x["type"] == "hello")
        replacement.send("RECONNECT " + tokens[acting])
        result = replacement.until(lambda x: x["type"] == "reconnected")
        assert result["seat"] == acting and result["lastRequest"] == play_id
        state = replacement.until(lambda x: x["type"] == "state")
        assert len(state["hand"]) == 6
        assert state["players"][acting]["connected"]
        assert state["players"][acting]["score"] == 1
        print("ServerProtocol: four clients, framing, privacy, authorization, deduplication and reconnect passed.")
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)
        for client in clients:
            client.close()
        server.stdout.close()
        server.stderr.close()

if __name__ == "__main__":
    main(sys.argv[1])
