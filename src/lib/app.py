from flask import Flask, request, jsonify, render_template
import sqlite3
from datetime import datetime

app = Flask(__name__)

# Database initialization
DB_FILE = "motion_logs.db"

@app.route("/")
def home():
    # Fetch motion logs from the database
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM motion_logs ORDER BY id DESC")
        logs = cursor.fetchall()

    # Render the logs in a basic UI
    return render_template("index.html", logs=logs)

@app.route('/api/resource', methods=['POST'])
def handle_post_request():
    try:
        # Parse JSON data from the request
        data = request.get_json()

        if not data:
            return jsonify({"error": "No JSON data received"}), 400

        # Extract fields from JSON
        timestamp = data.get("timestamp")

        if timestamp is None:
            return jsonify({"error": "Missing 'id' or 'timestamp' in the request"}), 400

        # Insert data into the database
        with sqlite3.connect(DB_FILE) as conn:
            cursor = conn.cursor()
            cursor.execute(
                "INSERT INTO motion_logs (timestamp) VALUES (?)",
                (timestamp,)
            )
            conn.commit()

        # Debug: Print confirmation
        print(f"Inserted into database: timestamp={timestamp}")

        # Respond to the client
        return jsonify({"message": "Data received successfully", "timestamp": timestamp}), 200

    except Exception as e:
        print(f"Error: {e}")  # Log error to the console
        return jsonify({"error": str(e)}), 500




if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
