#!/bin/bash
# Import Grafana dashboard manually
# Usage: ./import_dashboard.sh [grafana_url] [api_key]

GRAFANA_URL=${1:-"http://localhost:3000"}
API_KEY=${2:-""}
DASHBOARD_FILE="grafana/dashboard.json"

echo "=== Importing Rats Search Dashboard to Grafana ==="
echo "Grafana URL: $GRAFANA_URL"
echo ""

# Check if dashboard file exists
if [ ! -f "$DASHBOARD_FILE" ]; then
    echo "Error: Dashboard file not found at $DASHBOARD_FILE"
    exit 1
fi

# Build auth header
AUTH_HEADER=""
if [ -n "$API_KEY" ]; then
    AUTH_HEADER="-H \"Authorization: Bearer $API_KEY\""
else
    echo "No API key provided. Using basic auth (admin/Welcome)..."
    AUTH_HEADER="-u admin:Welcome"
fi

# Import dashboard
echo "Importing dashboard..."
RESPONSE=$(eval curl -s -w "\n%{http_code}" \
    -X POST \
    "$GRAFANA_URL/api/dashboards/import" \
    -H "Content-Type: application/json" \
    $AUTH_HEADER \
    -d "{
        \"dashboard\": $(cat $DASHBOARD_FILE),
        \"overwrite\": true,
        \"message\": \"Imported via script\"
    }")

HTTP_CODE=$(echo "$RESPONSE" | tail -n 1)
BODY=$(echo "$RESPONSE" | head -n -1)

if [ "$HTTP_CODE" = "200" ]; then
    echo "✓ Dashboard imported successfully!"
    echo "Response: $BODY"
    echo ""
    echo "Dashboard URL: $GRAFANA_URL/d/rats-search"
else
    echo "✗ Failed to import dashboard (HTTP $HTTP_CODE)"
    echo "Response: $BODY"
    exit 1
fi
