#!/bin/bash

# UR WebIF Landing Section WebSocket Backend Startup Script

echo "🚀 Starting UR WebIF Landing Section WebSocket Backend..."

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 is not installed. Please install Python 3 first."
    exit 1
fi

# Check if we're in the correct directory
if [ ! -f "websocket_backend.py" ]; then
    echo "❌ websocket_backend.py not found. Please run this script from the frontend directory."
    exit 1
fi

# Install Python dependencies if needed
if [ ! -d ".venv" ]; then
    echo "📦 Creating Python virtual environment..."
    python3 -m venv .venv
fi

echo "📦 Activating virtual environment..."
source .venv/bin/activate

echo "📦 Installing dependencies..."
pip install -r requirements.txt

# Create data directory if it doesn't exist
mkdir -p data

echo "🌐 Starting WebSocket backend server on ws://localhost:8765..."
echo "📊 Landing section data will be loaded from frontend/data/dashboard_data.json"
echo "🔄 The server will send periodic updates every 5 seconds"
echo "📈 Real-time metrics include: CPU, Memory, Network, Cellular, and Performance data"
echo ""
echo "📝 To test the connection:"
echo "   1. Open your web browser and navigate to the landing section"
echo "   2. The frontend should automatically connect to this backend"
echo "   3. Check the browser console for WebSocket connection logs"
echo "   4. Watch the dashboard metrics update in real-time"
echo ""
echo "⚠️  Press Ctrl+C to stop the server"
echo ""

# Start the WebSocket backend
python3 websocket_backend.py
