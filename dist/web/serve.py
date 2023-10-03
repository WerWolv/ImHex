import http.server
import os

class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):

    def end_headers(self):
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        http.server.SimpleHTTPRequestHandler.end_headers(self)

if __name__ == '__main__':
    os.chdir(".")
    httpd = http.server.HTTPServer(("", 9090), MyHttpRequestHandler)
    httpd.serve_forever()