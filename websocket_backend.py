#!/usr/bin/env python3
"""
WebSocket Backend Server for UR WebIF Frontend
Provides real-time data updates and API endpoints for dashboard components
"""

import asyncio
import websockets
import json
import logging
import os
from pathlib import Path
from datetime import datetime
import threading
import time

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class WebSocketBackend:
    def __init__(self):
        self.connected_clients = set()
        self.data_file = Path(__file__).parent / "data" / "dashboard_data.json"
        self.running = True
        self.update_interval = 5  # seconds
        
    async def register_client(self, websocket):
        """Register a new WebSocket client"""
        self.connected_clients.add(websocket)
        logger.info(f"Client connected. Total clients: {len(self.connected_clients)}")
        
        # Send initial data to the new client
        await self.send_initial_data(websocket)
        
    async def unregister_client(self, websocket):
        """Unregister a WebSocket client"""
        self.connected_clients.discard(websocket)
        logger.info(f"Client disconnected. Total clients: {len(self.connected_clients)}")
        
    async def send_initial_data(self, websocket):
        """Send initial dashboard data to a newly connected client"""
        try:
            data = self.load_dashboard_data()
            await websocket.send(json.dumps({
                "type": "initial_data",
                "data": data,
                "timestamp": datetime.now().isoformat()
            }))
        except Exception as e:
            logger.error(f"Error sending initial data: {e}")
            
    def load_dashboard_data(self):
        """Load dashboard data from JSON file"""
        try:
            if self.data_file.exists():
                with open(self.data_file, 'r') as f:
                    return json.load(f)
            else:
                # Return default data if file doesn't exist
                return self.get_default_data()
        except Exception as e:
            logger.error(f"Error loading dashboard data: {e}")
            return self.get_default_data()
            
    def get_default_data(self):
        """Get default landing section dashboard data"""
        return {
            "system_status": {
                "cpu_usage_percent": 45.2,
                "cpu_cores": 8,
                "cpu_temp_celsius": 42.5,
                "cpu_base_clock_ghz": 2.4,
                "cpu_boost_clock_ghz": 4.8,
                "ram_usage_percent": 62.8,
                "ram_used_gb": 5.1,
                "ram_total_gb": 8.0,
                "ram_available_gb": 2.9,
                "ram_type": "DDR4",
                "swap_usage_percent": 15.3,
                "swap_used_mb": 256,
                "swap_total_gb": 2.0,
                "swap_available_gb": 1.75,
                "swap_priority": "Normal",
                "uptime": "15 days, 7 hours",
                "last_updated": "2025-01-15T14:30:45Z"
            },
            "network_status": {
                "internet_status": "Connected",
                "internet_connected": True,
                "external_ip": "203.0.113.1",
                "dns_primary": "8.8.8.8",
                "dns_secondary": "8.8.4.4",
                "latency_ms": 23,
                "download_speed_mbps": 945,
                "upload_speed_mbps": 875,
                "server_status": "Connected",
                "server_connected": True,
                "server_hostname": "ur-webif.local",
                "server_port": "443",
                "server_protocol": "HTTPS",
                "last_ping_ms": 12,
                "session_duration": "2h 34m",
                "connection_type": "Ethernet",
                "interface_name": "eth0",
                "mac_address": "00:1A:2B:3C:4D:5E",
                "local_ip": "192.168.1.100",
                "gateway_ip": "192.168.1.1",
                "connection_speed": "1 Gbps"
            },
            "cellular": {
                "signal_status": "Excellent",
                "signal_bars": 5,
                "rssi_dbm": -65,
                "rsrp_dbm": -85,
                "rsrq_db": -8,
                "sinr_db": 15,
                "cell_id": "12345",
                "connection_status": "Connected",
                "is_connected": True,
                "network_name": "Verizon",
                "technology": "5G",
                "band": "n77",
                "apn": "vzwinternet",
                "data_usage_mb": 1024
            },
            "data_freshness": {
                "last_update": "2025-01-15T14:30:45Z",
                "update_interval": 30,
                "data_age_seconds": 0,
                "refresh_in_progress": False
            },
            "performance": {
                "load_average_1min": 1.2,
                "load_average_5min": 1.8,
                "load_average_15min": 2.1,
                "processes_running": 156,
                "processes_sleeping": 892,
                "processes_total": 1048
            }
        }
        
    async def broadcast_update(self, message):
        """Broadcast a message to all connected clients"""
        if self.connected_clients:
            disconnected_clients = set()
            for client in self.connected_clients:
                try:
                    await client.send(json.dumps(message))
                except websockets.exceptions.ConnectionClosed:
                    disconnected_clients.add(client)
                except Exception as e:
                    logger.error(f"Error sending message to client: {e}")
                    disconnected_clients.add(client)
            
            # Remove disconnected clients
            for client in disconnected_clients:
                self.connected_clients.discard(client)
                
    async def handle_client_message(self, websocket, message):
        """Handle incoming message from client"""
        try:
            data = json.loads(message)
            message_type = data.get("type")
            
            if message_type == "get_data":
                # Client requesting current data
                dashboard_data = self.load_dashboard_data()
                await websocket.send(json.dumps({
                    "type": "data_response",
                    "data": dashboard_data,
                    "timestamp": datetime.now().isoformat()
                }))
                
            elif message_type == "update_data":
                # Client wants to update data
                if "data" in data:
                    await self.save_dashboard_data(data["data"])
                    # Broadcast the update to all clients
                    await self.broadcast_update({
                        "type": "data_update",
                        "data": data["data"],
                        "timestamp": datetime.now().isoformat()
                    })
                    
            elif message_type == "refresh_data":
                # Client requesting data refresh
                await self.handle_system_command("refresh_data", {})
                
            elif message_type == "system_command":
                # Client sending system command
                command = data.get("command")
                await self.handle_system_command(command, data.get("params", {}))
                
        except json.JSONDecodeError:
            logger.error("Invalid JSON received from client")
        except Exception as e:
            logger.error(f"Error handling client message: {e}")
            
    async def save_dashboard_data(self, data):
        """Save dashboard data to JSON file"""
        try:
            # Ensure data directory exists
            self.data_file.parent.mkdir(parents=True, exist_ok=True)
            
            with open(self.data_file, 'w') as f:
                json.dump(data, f, indent=2)
            logger.info("Dashboard data saved successfully")
        except Exception as e:
            logger.error(f"Error saving dashboard data: {e}")
            
    async def handle_system_command(self, command, params):
        """Handle system commands from frontend"""
        logger.info(f"Received system command: {command}")
        
        if command == "restart_service":
            # Simulate service restart
            await self.broadcast_update({
                "type": "system_notification",
                "message": "Service restart initiated...",
                "level": "info",
                "timestamp": datetime.now().isoformat()
            })
            
        elif command == "system_shutdown":
            # Simulate system shutdown
            await self.broadcast_update({
                "type": "system_notification", 
                "message": "System shutdown initiated...",
                "level": "warning",
                "timestamp": datetime.now().isoformat()
            })
            
        elif command == "refresh_data":
            # Force immediate data refresh
            data = self.load_dashboard_data()
            await self.broadcast_update({
                "type": "data_update",
                "data": data,
                "timestamp": datetime.now().isoformat()
            })
            
    async def periodic_updates(self):
        """Send periodic updates to all clients"""
        while self.running:
            try:
                # Simulate real-time data updates for landing section
                data = self.load_dashboard_data()
                
                # Update system metrics with small random changes
                data["system_status"]["cpu_usage_percent"] = max(0, min(100, data["system_status"]["cpu_usage_percent"] + (time.time() % 10 - 5)))
                data["system_status"]["ram_usage_percent"] = max(0, min(100, data["system_status"]["ram_usage_percent"] + (time.time() % 8 - 4)))
                data["system_status"]["cpu_temp_celsius"] = max(30, min(60, data["system_status"]["cpu_temp_celsius"] + (time.time() % 3 - 1.5)))
                data["system_status"]["swap_usage_percent"] = max(0, min(100, data["system_status"]["swap_usage_percent"] + (time.time() % 6 - 3)))
                
                # Update network metrics
                data["network_status"]["latency_ms"] = max(5, min(100, int(20 + (time.time() % 50))))
                data["network_status"]["download_speed_mbps"] = max(100, min(1000, int(945 + (time.time() % 100 - 50))))
                data["network_status"]["upload_speed_mbps"] = max(50, min(1000, int(875 + (time.time() % 80 - 40))))
                data["network_status"]["last_ping_ms"] = max(5, min(50, int(12 + (time.time() % 20))))
                
                # Update cellular metrics
                data["cellular"]["rssi_dbm"] = max(-120, min(-50, int(data["cellular"]["rssi_dbm"] + (time.time() % 10 - 5))))
                data["cellular"]["signal_bars"] = max(1, min(5, int(5 - (abs(data["cellular"]["rssi_dbm"] + 85) / 14))))
                data["cellular"]["data_usage_mb"] += int((time.time() % 5) * 0.1)
                
                # Update performance metrics
                data["performance"]["load_average_1min"] = max(0.1, min(5.0, data["performance"]["load_average_1min"] + (time.time() % 2 - 1)))
                data["performance"]["processes_running"] = max(100, min(300, int(data["performance"]["processes_running"] + (time.time() % 20 - 10))))
                
                # Update data freshness
                current_time = datetime.now().isoformat()
                data["data_freshness"]["last_update"] = current_time
                data["system_status"]["last_updated"] = current_time
                
                await self.save_dashboard_data(data)
                await self.broadcast_update({
                    "type": "periodic_update",
                    "data": data,
                    "timestamp": current_time
                })
                
            except Exception as e:
                logger.error(f"Error in periodic updates: {e}")
                
            await asyncio.sleep(self.update_interval)
            
    async def handle_client(self, websocket, path):
        """Handle a WebSocket client connection"""
        await self.register_client(websocket)
        try:
            async for message in websocket:
                await self.handle_client_message(websocket, message)
        except websockets.exceptions.ConnectionClosed:
            pass
        except Exception as e:
            logger.error(f"Error handling client: {e}")
        finally:
            await self.unregister_client(websocket)
            
    async def start_server(self, host="localhost", port=8765):
        """Start the WebSocket server"""
        logger.info(f"Starting WebSocket server on {host}:{port}")
        
        # Start periodic updates task
        updates_task = asyncio.create_task(self.periodic_updates())
        
        # Start WebSocket server
        server = await websockets.serve(self.handle_client, host, port)
        
        try:
            await server.wait_closed()
        except KeyboardInterrupt:
            logger.info("Shutting down server...")
        finally:
            self.running = False
            updates_task.cancel()
            server.close()
            await server.wait_closed()

def main():
    """Main entry point"""
    backend = WebSocketBackend()
    
    # Create data directory and default data file
    data_dir = Path(__file__).parent / "data"
    data_dir.mkdir(exist_ok=True)
    
    default_data_file = data_dir / "dashboard_data.json"
    if not default_data_file.exists():
        default_data = backend.get_default_data()
        with open(default_data_file, 'w') as f:
            json.dump(default_data, f, indent=2)
        logger.info("Created default dashboard data file")
    
    # Start the server
    try:
        asyncio.run(backend.start_server())
    except KeyboardInterrupt:
        logger.info("Server stopped by user")

if __name__ == "__main__":
    main()
