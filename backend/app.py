from __future__ import annotations

import json
import os
import threading
import uuid
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, abort, jsonify, render_template, request, send_from_directory
from PIL import Image, UnidentifiedImageError


BASE_DIR = Path(__file__).resolve().parent
CAPTURES_DIR = BASE_DIR / "captures"
INDEX_FILE = CAPTURES_DIR / "index.json"
MAX_IMAGE_BYTES = 16 * 1024 * 1024
IMAGE_LOCK = threading.Lock()

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = MAX_IMAGE_BYTES


def read_index() -> list[dict]:
    if not INDEX_FILE.exists():
        return []
    try:
        data = json.loads(INDEX_FILE.read_text(encoding="utf-8"))
        return data if isinstance(data, list) else []
    except (OSError, json.JSONDecodeError):
        app.logger.exception("Could not read the capture index")
        return []


def write_index(items: list[dict]) -> None:
    CAPTURES_DIR.mkdir(parents=True, exist_ok=True)
    temporary = INDEX_FILE.with_suffix(".tmp")
    temporary.write_text(json.dumps(items, indent=2), encoding="utf-8")
    os.replace(temporary, INDEX_FILE)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def parse_capture_time(value: str | None) -> str:
    if value:
        try:
            parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
            if parsed.tzinfo is None:
                parsed = parsed.replace(tzinfo=timezone.utc)
            return parsed.astimezone(timezone.utc).isoformat(timespec="seconds")
        except ValueError:
            pass
    return utc_now()


def image_payload() -> tuple[bytes, str, str]:
    """Accept browser multipart uploads and raw image/jpeg camera POSTs."""
    if request.mimetype == "multipart/form-data":
        uploaded = request.files.get("image") or request.files.get("file")
        if uploaded is None:
            abort(400, description="Choose an image to upload.")
        payload = uploaded.read(MAX_IMAGE_BYTES + 1)
        original_name = uploaded.filename or "digicam-capture.jpg"
        capture_time = request.form.get("captured_at") or request.headers.get("X-Capture-Timestamp")
    else:
        payload = request.get_data(cache=False, as_text=False)
        original_name = request.headers.get("X-Filename", "digicam-capture.jpg")
        capture_time = request.headers.get("X-Capture-Timestamp")
    if not payload:
        abort(400, description="The uploaded image is empty.")
    if len(payload) > MAX_IMAGE_BYTES:
        abort(413, description="Images must be smaller than 16 MB.")
    return payload, Path(original_name).name, capture_time


@app.get("/")
def dashboard():
    return render_template("index.html")


@app.get("/api/health")
def health():
    return jsonify({"status": "ok", "service": "DIGICAM", "time": utc_now()})


@app.get("/api/captures")
def list_captures():
    items = sorted(read_index(), key=lambda item: item.get("captured_at", ""), reverse=True)
    total_bytes = sum(item.get("size_bytes", 0) for item in items)
    return jsonify({"captures": items, "count": len(items), "storage_bytes": total_bytes})


@app.post("/api/captures")
def save_capture():
    payload, original_name, capture_time = image_payload()
    try:
        from io import BytesIO

        with Image.open(BytesIO(payload)) as image:
            image.verify()
        with Image.open(BytesIO(payload)) as image:
            width, height = image.size
            if image.format != "JPEG":
                abort(415, description="Please upload a JPEG image.")
    except (UnidentifiedImageError, OSError, ValueError):
        abort(415, description="The upload is not a valid image.")

    captured_at = parse_capture_time(capture_time)
    date = datetime.fromisoformat(captured_at).astimezone(timezone.utc)
    folder = Path(f"{date:%Y}") / f"{date:%m}"
    capture_id = uuid.uuid4().hex
    filename = f"{date:%Y%m%d_%H%M%S}_{capture_id[:8]}.jpg"
    relative_path = folder / filename
    destination = CAPTURES_DIR / relative_path
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".part")
    temporary.write_bytes(payload)
    os.replace(temporary, destination)

    item = {
        "id": capture_id,
        "filename": filename,
        "original_name": original_name,
        "path": relative_path.as_posix(),
        "url": f"/captures/{relative_path.as_posix()}",
        "captured_at": captured_at,
        "size_bytes": len(payload),
        "width": width,
        "height": height,
        "format": "JPEG",
    }
    with IMAGE_LOCK:
        items = read_index()
        items.append(item)
        write_index(items)
    app.logger.info("Saved capture %s (%d bytes)", filename, len(payload))
    return jsonify({"capture": item}), 201


@app.delete("/api/captures/<capture_id>")
def delete_capture(capture_id: str):
    with IMAGE_LOCK:
        items = read_index()
        item = next((entry for entry in items if entry.get("id") == capture_id), None)
        if item is None:
            abort(404, description="Capture not found.")
        path = (CAPTURES_DIR / item["path"]).resolve()
        if CAPTURES_DIR.resolve() not in path.parents:
            abort(400, description="Invalid capture path.")
        try:
            path.unlink(missing_ok=True)
        except OSError:
            app.logger.exception("Could not delete capture %s", capture_id)
            abort(500, description="Could not delete the image from disk.")
        write_index([entry for entry in items if entry.get("id") != capture_id])
    return jsonify({"deleted": capture_id})


@app.get("/captures/<path:filename>")
def serve_capture(filename: str):
    return send_from_directory(CAPTURES_DIR, filename, conditional=True, max_age=3600)


@app.errorhandler(413)
def too_large(_error):
    return jsonify({"error": "Images must be smaller than 16 MB."}), 413


@app.errorhandler(400)
@app.errorhandler(404)
@app.errorhandler(415)
@app.errorhandler(500)
def api_error(error):
    if request.path.startswith("/api/"):
        return jsonify({"error": getattr(error, "description", "The request could not be completed.")}), error.code
    return error


if __name__ == "__main__":
    CAPTURES_DIR.mkdir(parents=True, exist_ok=True)
    app.run(host="0.0.0.0", port=5000, debug=False)
