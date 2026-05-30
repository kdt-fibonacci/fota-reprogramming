from flask import Flask
from flask import render_template
from flask import request
from flask import redirect
from datetime import datetime

import os
import requests

app = Flask(__name__)

# =========================
# CONFIG
# =========================

OTA_SERVER = "http://192.168.202.106:4321"

UPLOAD_DIR = "./uploads"

os.makedirs(
    UPLOAD_DIR,
    exist_ok=True
)

ECU_NAME = {
    "1234": "MOTOR ECU",
    "5678": "STEERING ECU"
}

ota_logs = []

# =========================
# MAIN PAGE
# =========================

@app.route("/")
def index():

    ecus = []

    try:
        res = requests.post(
            OTA_SERVER + "/ota/check",
            json={
                "device_id": "0001",
                "ecus": [
                    {
                        "address": "1234",
                        "version": "0.0"
                    },
                    {
                        "address": "5678",
                        "version": "0.0"
                    }
                ]
            },
            timeout=3
        )

        data = res.json()

        updates = data.get("updates", [])

        ecus = [
            {
                "address": u["address"],
                "name": ECU_NAME.get(
                    u["address"],
                    "UNKNOWN"
                ),
                "version": u.get(
                    "version",
                    "0.0"
                ),
                "update_required": u.get(
                    "update_required",
                    False
                )
            }
            for u in updates
        ]

    except Exception as e:
        print("\n[OTA CHECK ERROR]", e)

    return render_template(
        "index.html",
        ecus=ecus,
        logs= ota_logs
    )

# =========================
# UPLOAD PAGE
# =========================

@app.route("/upload")
def upload_page():

    return render_template(
        "upload.html",
        ecus=ECU_NAME
    )

# =========================
# UPLOAD ACTION
# =========================

@app.route(
    "/upload",
    methods=["POST"]
)
def upload_action():

    ecu = request.form["ecu"]

    version = request.form["version"]

    hex_file = request.files["hex_file"]

    try:

        files = {
            "hex_file": (
                hex_file.filename,
                hex_file.stream,
                hex_file.mimetype
            )
        }

        data = {
            "address": ecu,
            "version": version
        }

        res = requests.post(
            OTA_SERVER + "/upload",
            data=data,
            files=files,
            timeout=30
        )

        print("\n==============================")
        print("[OTA UPLOAD RESPONSE]")
        print("STATUS :", res.status_code)
        print("BODY   :", res.text)
        print("==============================")

        ota_logs.insert(0, {

        "time":
            datetime.now().strftime(
                "%H:%M:%S"
            ),

        "ecu_address":
            ecu,

        "version":
            version,

        "status":
            "UPLOAD",

        "error_code":
            "0x00"
        })

    except Exception as e:

        print("\n[UPLOAD ERROR]", e)

    return redirect("/")


@app.route(
    "/api/log",
    methods=["POST"]
)
def add_log():

    data = request.json

    data["time"] = datetime.now().strftime(
        "%H:%M:%S"
    )

    ota_logs.insert(0, data)

    ota_logs[:] = ota_logs[:100]

    return {
        "result": "ok"
    }

@app.route("/api/logs")
def api_logs():

    return {
        "logs": ota_logs
    }

# =========================
# MAIN
# =========================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )