from http.server import SimpleHTTPRequestHandler, HTTPServer

PORT = 8000

class CustomHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        SimpleHTTPRequestHandler.end_headers(self)

try:
    with HTTPServer(('localhost', PORT), CustomHandler) as httpd:
        print(f"Serving at http://localhost:{PORT}/")
        httpd.serve_forever()
except KeyboardInterrupt:
    print("\nServer stopped.")
