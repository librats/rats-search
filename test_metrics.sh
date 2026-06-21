#!/bin/bash
# Test script for Rats Search metrics endpoint
# Usage: ./test_metrics.sh [port]

PORT=${1:-8095}
BASE_URL="http://localhost:$PORT"

echo "=== Testing Rats Search Metrics Endpoint ==="
echo "Target: $BASE_URL"
echo ""

# Test 1: Health check
echo "1. Testing /healthz endpoint..."
HEALTHZ_RESPONSE=$(curl -s -w "\n%{http_code}" "$BASE_URL/healthz")
HEALTHZ_BODY=$(echo "$HEALTHZ_RESPONSE" | head -n -1)
HEALTHZ_CODE=$(echo "$HEALTHZ_RESPONSE" | tail -n 1)

if [ "$HEALTHZ_CODE" = "200" ]; then
    echo "   ✓ Health check passed (HTTP $HEALTHZ_CODE)"
    echo "   Response: $HEALTHZ_BODY"
else
    echo "   ✗ Health check failed (HTTP $HEALTHZ_CODE)"
    echo "   Response: $HEALTHZ_BODY"
fi
echo ""

# Test 2: Readiness check
echo "2. Testing /readyz endpoint..."
READYZ_RESPONSE=$(curl -s -w "\n%{http_code}" "$BASE_URL/readyz")
READYZ_BODY=$(echo "$READYZ_RESPONSE" | head -n -1)
READYZ_CODE=$(echo "$READYZ_RESPONSE" | tail -n 1)

if [ "$READYZ_CODE" = "200" ] || [ "$READYZ_CODE" = "503" ]; then
    echo "   ✓ Readiness check responded (HTTP $READYZ_CODE)"
    echo "   Response: $READYZ_BODY"
else
    echo "   ✗ Readiness check failed (HTTP $READYZ_CODE)"
    echo "   Response: $READYZ_BODY"
fi
echo ""

# Test 3: Metrics endpoint
echo "3. Testing /metrics endpoint..."
METRICS_RESPONSE=$(curl -s -w "\n%{http_code}" "$BASE_URL/metrics")
METRICS_BODY=$(echo "$METRICS_RESPONSE" | head -n -1)
METRICS_CODE=$(echo "$METRICS_RESPONSE" | tail -n 1)

if [ "$METRICS_CODE" = "200" ]; then
    echo "   ✓ Metrics endpoint working (HTTP $METRICS_CODE)"
    echo ""
    echo "   Metrics output:"
    echo "$METRICS_BODY" | head -50
    
    # Count metrics
    METRIC_COUNT=$(echo "$METRICS_BODY" | grep -c "^rats_" || true)
    echo ""
    echo "   Total metrics found: $METRIC_COUNT"
else
    echo "   ✗ Metrics endpoint failed (HTTP $METRICS_CODE)"
    echo "   Response: $METRICS_BODY"
fi
echo ""

# Test 4: Check for specific metric families
echo "4. Checking for required metric families..."
REQUIRED_METRICS=(
    "rats_server_uptime_seconds"
    "rats_websocket_connections"
    "rats_http_server_running"
    "rats_http_requests_total"
    "rats_p2p_peer_count"
    "rats_p2p_dht_node_count"
    "rats_db_torrents_total"
    "rats_db_files_total"
    "rats_db_size_bytes"
    "rats_downloads_active"
)

MISSING_METRICS=()
for metric in "${REQUIRED_METRICS[@]}"; do
    if echo "$METRICS_BODY" | grep -q "^${metric} "; then
        echo "   ✓ $metric"
    else
        echo "   ✗ $metric (MISSING)"
        MISSING_METRICS+=("$metric")
    fi
done
echo ""

# Summary
echo "=== Test Summary ==="
if [ "$HEALTHZ_CODE" = "200" ] && [ "$METRICS_CODE" = "200" ]; then
    echo "✓ All basic tests passed"
    if [ ${#MISSING_METRICS[@]} -eq 0 ]; then
        echo "✓ All required metrics present"
    else
        echo "✗ Missing ${#MISSING_METRICS[@]} metrics: ${MISSING_METRICS[*]}"
    fi
else
    echo "✗ Some tests failed"
    exit 1
fi
