#!/bin/bash

# Variables
DATABASE_NAME="EVChargingDB"
MYSQL_USER="root"
MYSQL_PASSWORD="root"

# Helper function to execute MySQL commands
mysql_cmd() {
   sudo mysql -u"$MYSQL_USER" -p"$MYSQL_PASSWORD" -e "$1"
}

# 1. Check if MySQL is running; start it if not
echo "Checking MySQL status..."
if ! systemctl is-active --quiet mysql; then
    echo "MySQL is not active. Starting it..."
    sudo systemctl start mysql
else
    echo "MySQL is already running."
fi

mysql_cmd "ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY '${MYSQL_PASSWORD}';"
# 2. Recreate the database and tables
echo "Recreating database \"$DATABASE_NAME\"..."

echo "Dropping existing database..."
mysql_cmd "DROP DATABASE IF EXISTS ${DATABASE_NAME};"

echo "Creating new database..."
mysql_cmd "CREATE DATABASE IF NOT EXISTS ${DATABASE_NAME};"

echo "Creating table \"nodes\"..."
mysql_cmd "CREATE TABLE IF NOT EXISTS ${DATABASE_NAME}.nodes (
    ip VARCHAR(255) NOT NULL,
    name VARCHAR(255) NOT NULL,
    resource VARCHAR(255) NOT NULL,
    settings VARCHAR(255) NOT NULL,
    PRIMARY KEY (ip)
);"

echo "Creating table \"energy\"..."
mysql_cmd "CREATE TABLE IF NOT EXISTS ${DATABASE_NAME}.energy (
    timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    predicted FLOAT DEFAULT NULL,
    sampled FLOAT DEFAULT NULL,
    PRIMARY KEY (timestamp)
);"

echo "Creating table \"vehicles\"..."
mysql_cmd "CREATE TABLE IF NOT EXISTS ${DATABASE_NAME}.vehicles (
    uid INT NOT NULL,
    id INT NOT NULL,
    battery INT NOT NULL,
    priority INT NOT NULL,
    charging INT NOT NULL,
    PRIMARY KEY (uid)
);"

echo "Database and tables created successfully!"

# 3. Start the Python server and client in separate terminals
echo "Launching CoAP server in a new terminal..."
gnome-terminal -- bash -c "echo 'Starting server.py...'; python3 server.py; echo 'Server exited, press Enter to close...'; read"

sleep 2

echo "Launching CLI client in a new terminal..."
gnome-terminal -- bash -c "echo 'Starting client.py...'; python3 client.py; echo 'Client exited, press Enter to close...'; read"

echo "Done. You should now have two terminals open."
