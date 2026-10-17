#!/usr/bin/env python3
"""Verify TLS acceptance and rejection using the actual frozen executable."""
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading


def main(executable):
    with tempfile.TemporaryDirectory(prefix='plus-tls-test-') as folder:
        root = Path(folder)
        cert, key = root / 'server.pem', root / 'server.key'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-keyout', str(key), '-out', str(cert), '-days', '1',
                        '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost'],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'verified local TLS fixture')
            def log_message(self, *args):
                pass
        server = http.server.HTTPServer(('127.0.0.1', 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            env = {k: v for k, v in os.environ.items() if k not in ('SSL_CERT_FILE', 'SSL_CERT_DIR')}
            env['NO_PROXY'] = 'localhost,127.0.0.1'
            url = 'https://localhost:' + str(server.server_port)
            tests = [
                ('trusted certificate', url, dict(env, SSL_CERT_FILE=str(cert)), True),
                ('untrusted certificate', url, env, False),
                ('hostname mismatch', url.replace('localhost', '127.0.0.1'), dict(env, SSL_CERT_FILE=str(cert)), False),
            ]
            for name, target, environment, success in tests:
                result = subprocess.run([executable, '--tls-check', target], env=environment,
                                        text=True, capture_output=True, timeout=15)
                if (result.returncode == 0) != success:
                    raise RuntimeError(name + ': ' + result.stdout + result.stderr)
                if not success and 'CERTIFICATE_VERIFY_FAILED' not in result.stderr:
                    raise RuntimeError('Expected certificate rejection: ' + result.stderr)
                print('Frozen TLS test passed: ' + name, flush=True)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == '__main__':
    main(sys.argv[1])
