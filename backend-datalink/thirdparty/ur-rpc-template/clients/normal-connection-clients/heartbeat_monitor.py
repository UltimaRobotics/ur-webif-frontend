
#!/usr/bin/env python3
"""
Heartbeat Monitor Script
Listens to MQTT heartbeat topic and logs reception time of each message
"""

import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime
import sys

# MQTT Configuration
BROKER_HOST = "localhost"
BROKER_PORT = 1899
HEARTBEAT_TOPIC = "clients/ur-mavrouter/heartbeat"
CLIENT_ID = "heartbeat_monitor"

# Log file
LOG_FILE = "heartbeat_log.txt"

def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Connected to MQTT broker")
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Subscribing to topic: {HEARTBEAT_TOPIC}")
        client.subscribe(HEARTBEAT_TOPIC)
        
        # Log to file
        with open(LOG_FILE, "a") as f:
            f.write(f"\n{'='*80}\n")
            f.write(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Monitor started\n")
            f.write(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Subscribed to: {HEARTBEAT_TOPIC}\n")
            f.write(f"{'='*80}\n")
    else:
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Connection failed with code {rc}")

def on_message(client, userdata, msg):
    """Callback when a message is received"""
    reception_time = datetime.now()
    timestamp_str = reception_time.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
    
    try:
        # Try to parse JSON payload
        payload = json.loads(msg.payload.decode('utf-8'))
        payload_str = json.dumps(payload, indent=2)
        
        # Extract timestamp from payload if available
        msg_timestamp = payload.get('timestamp', 'N/A')
        
        # Console output
        print(f"\n{'─'*80}")
        print(f"[RECEPTION TIME] {timestamp_str}")
        print(f"[MESSAGE TIMESTAMP] {msg_timestamp}")
        print(f"[TOPIC] {msg.topic}")
        print(f"[PAYLOAD]")
        print(payload_str)
        print(f"{'─'*80}")
        
        # File logging
        with open(LOG_FILE, "a") as f:
            f.write(f"\n[RECEPTION TIME] {timestamp_str}\n")
            f.write(f"[MESSAGE TIMESTAMP] {msg_timestamp}\n")
            f.write(f"[TOPIC] {msg.topic}\n")
            f.write(f"[PAYLOAD] {payload_str}\n")
            f.write(f"{'-'*80}\n")
            
    except json.JSONDecodeError:
        # If not JSON, log as raw text
        payload_str = msg.payload.decode('utf-8', errors='replace')
        
        print(f"\n{'─'*80}")
        print(f"[RECEPTION TIME] {timestamp_str}")
        print(f"[TOPIC] {msg.topic}")
        print(f"[RAW PAYLOAD] {payload_str}")
        print(f"{'─'*80}")
        
        with open(LOG_FILE, "a") as f:
            f.write(f"\n[RECEPTION TIME] {timestamp_str}\n")
            f.write(f"[TOPIC] {msg.topic}\n")
            f.write(f"[RAW PAYLOAD] {payload_str}\n")
            f.write(f"{'-'*80}\n")

def on_disconnect(client, userdata, rc):
    """Callback when disconnected from MQTT broker"""
    timestamp_str = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
    print(f"\n[{timestamp_str}] Disconnected from MQTT broker (rc={rc})")
    
    with open(LOG_FILE, "a") as f:
        f.write(f"\n[{timestamp_str}] Disconnected (rc={rc})\n")

def main():
    """Main function to run the heartbeat monitor"""
    print(f"Heartbeat Monitor")
    print(f"{'='*80}")
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"Topic: {HEARTBEAT_TOPIC}")
    print(f"Log File: {LOG_FILE}")
    print(f"{'='*80}\n")
    
    # Create MQTT client
    client = mqtt.Client(client_id=CLIENT_ID)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        # Connect to broker
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Connecting to broker...")
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
        
        # Start the loop
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Starting monitoring loop...")
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] Press Ctrl+C to stop\n")
        
        client.loop_forever()
        
    except KeyboardInterrupt:
        timestamp_str = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        print(f"\n\n[{timestamp_str}] Stopping monitor...")
        
        with open(LOG_FILE, "a") as f:
            f.write(f"\n[{timestamp_str}] Monitor stopped by user\n")
            f.write(f"{'='*80}\n\n")
        
        client.disconnect()
        sys.exit(0)
        
    except Exception as e:
        timestamp_str = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        print(f"\n[{timestamp_str}] Error: {e}")
        
        with open(LOG_FILE, "a") as f:
            f.write(f"\n[{timestamp_str}] Error: {e}\n")
            f.write(f"{'='*80}\n\n")
        
        sys.exit(1)

if __name__ == "__main__":
    main()
