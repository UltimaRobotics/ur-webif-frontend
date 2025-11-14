#!/usr/bin/env python3
"""
UR WebIF Frontend Demo Server
Serves the standalone frontend with mock API endpoints
No backend required - all data is simulated
"""

import http.server
import socketserver
import os
import json
from pathlib import Path
from urllib.parse import urlparse, parse_qs
import time
import mimetypes

PORT = 8080
DIRECTORY = Path(__file__).parent

class FrontendDemoHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DIRECTORY), **kwargs)
    
    def end_headers(self):
        # Add CORS headers
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization')
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate')
        
        # Fix Content Security Policy to allow inline scripts and external resources
        if self.path.endswith('.html'):
            csp = (
                "default-src 'self'; "
                "script-src 'self' 'unsafe-inline' 'unsafe-eval' "
                "https://cdn.tailwindcss.com "
                "https://cdnjs.cloudflare.com "
                "https://fonts.googleapis.com; "
                "style-src 'self' 'unsafe-inline' "
                "https://fonts.googleapis.com; "
                "font-src 'self' https://fonts.gstatic.com; "
                "img-src 'self' data:; "
                "connect-src 'self'; "
                "frame-src 'self';"
            )
            self.send_header('Content-Security-Policy', csp)
        
        super().end_headers()
    
    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()
    
    def do_GET(self):
        # Handle API endpoints
        if self.path.startswith('/api/'):
            self.handle_api_request()
        else:
            # Serve static files with proper MIME types
            self.serve_static_file()
    
    def do_POST(self):
        if self.path.startswith('/api/'):
            self.handle_api_request()
        else:
            self.send_error(404)
    
    def serve_static_file(self):
        """Serve static files with proper MIME types and headers"""
        try:
            # Convert URL path to filesystem path
            file_path = self.translate_path(self.path)
            
            # Security check - ensure we're within the directory
            if not os.path.abspath(file_path).startswith(os.path.abspath(DIRECTORY)):
                self.send_error(403, "Forbidden")
                return
            
            # Check if file exists
            if not os.path.exists(file_path):
                self.send_error(404, "File not found")
                return
            
            # Get MIME type
            mime_type, _ = mimetypes.guess_type(file_path)
            if mime_type is None:
                if file_path.endswith('.js'):
                    mime_type = 'application/javascript'
                elif file_path.endswith('.css'):
                    mime_type = 'text/css'
                elif file_path.endswith('.html'):
                    mime_type = 'text/html'
                else:
                    mime_type = 'application/octet-stream'
            
            # Read and serve file
            with open(file_path, 'rb') as f:
                content = f.read()
            
            self.send_response(200)
            self.send_header('Content-Type', mime_type)
            self.send_header('Content-Length', str(len(content)))
            
            # Add caching headers for static assets
            if file_path.endswith(('.js', '.css', '.png', '.jpg', '.jpeg', '.gif', '.ico')):
                self.send_header('Cache-Control', 'public, max-age=3600')
            
            self.end_headers()
            self.wfile.write(content)
            
            # Log successful file serving
            print(f"[STATIC] GET {self.path} -> 200 OK ({mime_type})")
            
        except Exception as e:
            print(f"[ERROR] Failed to serve {self.path}: {e}")
            self.send_error(500, "Internal server error")
    
    def translate_path(self, path):
        """Translate URL path to filesystem path"""
        # Remove query parameters and fragments
        path = path.split('?', 1)[0]
        path = path.split('#', 1)[0]
        
        # Remove leading slash
        if path.startswith('/'):
            path = path[1:]
        
        # Join with directory
        return os.path.join(DIRECTORY, path)
    
    def handle_api_request(self):
        """Handle mock API requests"""
        path = self.path.split('?')[0]
        
        # Parse request body for POST requests
        content_length = int(self.headers.get('Content-Length', 0))
        request_body = {}
        if content_length > 0:
            try:
                request_body = json.loads(self.rfile.read(content_length).decode('utf-8'))
            except:
                pass
        
        # Mock API responses
        mock_endpoints = {
            '/api/login': {
                'method': 'POST',
                'response': {
                    'success': True,
                    'message': 'Login successful (DEMO MODE)',
                    'session_token': f'demo_token_{int(time.time())}',
                    'user': {
                        'username': request_body.get('username', 'demo_user'),
                        'role': 'administrator',
                        'email': 'demo@example.com'
                    }
                }
            },
            '/api/dashboard-data': {
                'method': 'GET',
                'response': {
                    'success': True,
                    'system': {
                        'hostname': 'ur-webif-demo',
                        'uptime': 86400 + int(time.time()) % 10000,
                        'cpu_usage': 45.2 + (int(time.time()) % 20),
                        'memory_usage': 62.8 + (int(time.time()) % 10),
                        'disk_usage': 38.5 + (int(time.time()) % 5),
                        'temperature': 45.0 + (int(time.time()) % 8),
                        'kernel': 'Linux 5.15.0-demo'
                    },
                    'network': {
                        'eth0': {
                            'status': 'up',
                            'ip': '192.168.1.100',
                            'mac': '00:11:22:33:44:55',
                            'rx_bytes': 1024000 + int(time.time()) % 100000,
                            'tx_bytes': 512000 + int(time.time()) % 50000
                        },
                        'wlan0': {
                            'status': 'up',
                            'ip': '192.168.1.101',
                            'mac': '00:11:22:33:44:66',
                            'rx_bytes': 512000 + int(time.time()) % 50000,
                            'tx_bytes': 256000 + int(time.time()) % 25000
                        }
                    },
                    'cellular': {
                        'status': 'connected',
                        'signal_strength': 85 + (int(time.time()) % 10),
                        'operator': 'Demo Carrier',
                        'technology': '4G LTE',
                        'imei': '123456789012345'
                    }
                }
            },
            '/api/health': {
                'method': 'GET',
                'response': {
                    'status': 'healthy',
                    'version': '1.0.0-demo',
                    'mode': 'frontend-only',
                    'timestamp': int(time.time())
                }
            },
            '/api/license/status': {
                'method': 'GET',
                'response': {
                    'success': True,
                    'licensed': True,
                    'license_type': 'Demo License',
                    'expires': '2025-12-31',
                    'features': ['all']
                }
            },
            '/api/vpn/status': {
                'method': 'GET',
                'response': {
                    'success': True,
                    'vpn_connections': [
                        {
                            'name': 'VPN Demo 1',
                            'status': 'connected',
                            'type': 'OpenVPN',
                            'ip': '10.8.0.1',
                            'connected_since': int(time.time()) - 3600
                        },
                        {
                            'name': 'VPN Demo 2',
                            'status': 'disconnected',
                            'type': 'WireGuard',
                            'ip': None,
                            'connected_since': None
                        }
                    ]
                }
            },
            '/api/wireless/status': {
                'method': 'GET',
                'response': {
                    'success': True,
                    'access_points': [
                        {
                            'ssid': 'Demo-WiFi',
                            'channel': 6,
                            'clients': 5,
                            'status': 'active',
                            'frequency': '2.4GHz'
                        }
                    ]
                }
            },
            '/api/system/stats': {
                'method': 'GET',
                'response': {
                    'success': True,
                    'cpu': {
                        'usage': 45.2 + (int(time.time()) % 20),
                        'cores': 4,
                        'frequency': '1.8 GHz'
                    },
                    'memory': {
                        'usage': 62.8 + (int(time.time()) % 10),
                        'total': 4096,
                        'used': 2560,
                        'free': 1536
                    },
                    'disk': {
                        'usage': 38.5 + (int(time.time()) % 5),
                        'total': 32768,
                        'used': 12614,
                        'free': 20154
                    },
                    'temperature': 45.0 + (int(time.time()) % 8)
                }
            }
        }
        
        # Get response or return default
        endpoint_config = mock_endpoints.get(path)
        if endpoint_config:
            response_data = endpoint_config['response']
        else:
            response_data = {
                'success': True,
                'message': 'Mock API response',
                'path': path,
                'mode': 'demo'
            }
        
        # Send response
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(response_data).encode())
        
        # Log request
        print(f"[API] {self.command} {path} -> 200 OK")
    
    def log_message(self, format, *args):
        # Custom logging
        if self.path.startswith('/api/'):
            # API requests logged in handle_api_request
            return
        if not self.path.startswith('/favicon.ico'):
            print(f"[{self.log_date_time_string()}] {format % args}")

def main():
    print(f"""
╔══════════════════════════════════════════════════════════════╗
║     UR WebIF Frontend Demo Server                           ║
╠══════════════════════════════════════════════════════════════╣
║  Mode:         Frontend-Only (No Backend Required)          ║
║  URL:          http://localhost:{PORT}                          ║
║  Directory:    {str(DIRECTORY)[:40]}{'...' if len(str(DIRECTORY)) > 40 else ''}
║  Mock API:     Enabled                                       ║
║  Login:        Any username/password works                   ║
║  CSP:          Updated for inline scripts & externals        ║
╚══════════════════════════════════════════════════════════════╝

📝 This is a demo server with mock data
🚀 Open http://localhost:{PORT}/login-page.html in your browser
⚠️  All API calls return simulated data
🔓 Authentication is bypassed - any credentials work
🔧 Content Security Policy updated for JavaScript modules
""")
    
    with socketserver.TCPServer(("", PORT), FrontendDemoHandler) as httpd:
        print(f"✅ Server started successfully!")
        print(f"🌐 Listening on http://localhost:{PORT}")
        print(f"📁 Serving files from: {DIRECTORY}")
        print(f"\nPress Ctrl+C to stop the server\n")
        print("=" * 64)
        
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n\n🛑 Server stopped by user")
            httpd.shutdown()

if __name__ == "__main__":
    main()

