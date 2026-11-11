"""HTTP and TCP echo servers for host_test. Prints "<http-port> <echo-port>"."""
import http.server
import socket
import socketserver
import sys
import threading

BIG = bytes(ord("a") + i % 26 for i in range(3000))


class Http(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, code, body, length=True):
        self.send_response(code)
        if length:
            self.send_header("Content-Length", str(len(body)))
        # A long header line, past the core's line buffer.
        self.send_header("X-Padding", "p" * 1500)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/big":
            self.reply(200, BIG)
        elif self.path == "/nolength":
            self.reply(200, b"until close", length=False)
        else:
            self.reply(404, b"no")

    def do_POST(self):
        self.reply(200, self.rfile.read(int(self.headers["Content-Length"])))


class Echo(socketserver.StreamRequestHandler):
    def handle(self):
        for line in self.rfile:
            if line.strip() == b"quit":
                return
            self.wfile.write(line)
            self.wfile.flush()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


http_server = Server(("127.0.0.1", 0), Http)
echo_server = Server(("127.0.0.1", 0), Echo)
threading.Thread(target=echo_server.serve_forever, daemon=True).start()
print(http_server.server_address[1], echo_server.server_address[1], flush=True)
http_server.serve_forever()
