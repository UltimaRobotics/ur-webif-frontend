# Backend-Datalink RPC Client Integration

This document describes the RPC client functionality integrated into the backend-datalink service using the ur-rpc-template and ur-threadder-api libraries.

## Overview

The backend-datalink service now includes MQTT-based RPC communication capabilities, allowing remote procedure calls to be processed asynchronously. The integration follows the patterns from the ur-rpc-template integration guide.

## Configuration

### Command Line Arguments

The backend-datalink service now requires an additional RPC configuration parameter:

```bash
./backend-datalink -pkg_config config/config.json -rpc_config config/rpc_config.json
```

- `-pkg_config`: Path to the main service configuration file (existing parameter)
- `-rpc_config`: Path to the RPC client configuration file (new parameter)

### RPC Configuration File

The RPC configuration file (`config/rpc_config.json`) contains MQTT broker settings and topic configuration:

```json
{
  "client_id": "backend-datalink",
  "broker_host": "127.0.0.1",
  "broker_port": 1899,
  "keepalive": 60,
  "qos": 1,
  "auto_reconnect": true,
  "reconnect_delay_min": 1,
  "reconnect_delay_max": 60,
  "use_tls": false,
  "heartbeat": {
    "enabled": true,
    "interval_seconds": 5,
    "topic": "clients/backend-datalink/heartbeat",
    "payload": "{\"client\":\"backend-datalink\",\"status\":\"alive\"}"
  },
  "json_added_pubs": {
    "topics": [
      "direct_messaging/backend-datalink/responses"
    ]
  },
  "json_added_subs": {
    "topics": [
      "direct_messaging/backend-datalink/requests"
    ]
  }
}
```

## Supported RPC Methods

The RPC client supports the following methods for remote procedure calls:

### 1. get_dashboard_data

Retrieves dashboard data from the database.

**Parameters:**
```json
{
  "categories": ["system", "ram", "network"] // Optional array of categories
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "transaction_id",
  "result": {
    "data": {
      "system": { ... },
      "ram": { ... },
      "network": { ... }
    },
    "timestamp": 1634567890,
    "message": "Dashboard data retrieved successfully"
  }
}
```

### 2. get_system_status

Returns the status of all backend-datalink components.

**Parameters:** None

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "transaction_id",
  "result": {
    "rpc_client": {
      "status": "running",
      "timestamp": 1634567890
    },
    "websocket_server": {
      "running": true,
      "status": "active"
    },
    "database": {
      "initialized": true,
      "status": "connected"
    },
    "system_collector": {
      "running": true,
      "status": "active"
    },
    "network_priority_manager": {
      "running": true,
      "status": "active"
    },
    "message": "System status retrieved successfully"
  }
}
```

### 3. get_network_info

Retrieves network priority and routing information.

**Parameters:** None

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "transaction_id",
  "result": {
    "interfaces": [ ... ],
    "routing_rules": [ ... ],
    "timestamp": 1634567890,
    "message": "Network information retrieved successfully"
  }
}
```

### 4. broadcast_message

Broadcasts a message to all connected WebSocket clients.

**Parameters:**
```json
{
  "message": "Hello from RPC",
  "type": "notification" // Optional, defaults to "broadcast"
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "transaction_id",
  "result": {
    "success": true,
    "message": "Message broadcast successfully",
    "broadcast_type": "notification",
    "original_message": "Hello from RPC",
    "timestamp": 1634567890
  }
}
```

## Message Format

All RPC requests must follow the JSON-RPC 2.0 specification:

```json
{
  "jsonrpc": "2.0",
  "method": "method_name",
  "params": { ... },
  "id": "transaction_id"
}
```

## Topics

The RPC client uses the following MQTT topics:

- **Requests:** `direct_messaging/backend-datalink/requests`
- **Responses:** `direct_messaging/backend-datalink/responses`
- **Heartbeat:** `clients/backend-datalink/heartbeat`

## Architecture

### Components

1. **RpcClient**: Handles MQTT communication and message routing
2. **RpcOperationProcessor**: Processes RPC requests asynchronously using thread pools
3. **ThreadManager**: Manages thread lifecycle and resource cleanup

### Thread Safety

- All RPC operations are processed in separate threads
- Thread-safe access to global components is ensured
- Proper cleanup of resources during shutdown

### Error Handling

- Comprehensive error handling with detailed error messages
- Graceful degradation when components are unavailable
- Automatic reconnection to MQTT broker

## Building

The RPC integration requires the following dependencies:

- ur-rpc-template (included in thirdparty/)
- ur-threadder-api (included in thirdparty/)
- mosquitto (MQTT client library)
- cJSON (JSON parsing library)

The CMakeLists.txt has been updated to automatically include these dependencies.

## Example Usage

1. Start the backend-datalink service with RPC configuration:
   ```bash
   ./backend-datalink -pkg_config config/config.json -rpc_config config/rpc_config.json
   ```

2. Send an RPC request via MQTT:
   ```bash
   mosquitto_pub -h 127.0.0.1 -p 1899 -t "direct_messaging/backend-datalink/requests" -m '{
     "jsonrpc": "2.0",
     "method": "get_system_status",
     "params": {},
     "id": "test_001"
   }'
   ```

3. Listen for responses:
   ```bash
   mosquitto_sub -h 127.0.0.1 -p 1899 -t "direct_messaging/backend-datalink/responses"
   ```

## Logging

The RPC client provides detailed logging for:
- Connection status changes
- Message processing
- Error conditions
- Thread lifecycle events

Enable verbose logging by setting the verbose flag in the RpcOperationProcessor constructor.

## Troubleshooting

### Common Issues

1. **RPC client fails to start**
   - Check that the MQTT broker is running on the specified host and port
   - Verify the RPC configuration file syntax
   - Ensure required dependencies are available

2. **No response to RPC requests**
   - Check that requests are sent to the correct topic
   - Verify JSON-RPC 2.0 message format
   - Check service logs for error messages

3. **Connection timeouts**
   - Verify network connectivity to MQTT broker
   - Check firewall settings
   - Review broker configuration

### Debug Mode

Enable debug logging by rebuilding with debug flags and checking the console output for detailed RPC client information.
