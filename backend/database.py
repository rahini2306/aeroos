"""SQLite persistence; JSON payloads retain full reproducible inputs and engine outputs."""
import json
import os
import sqlite3
from contextlib import contextmanager
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DB = Path(os.environ.get("AEROOS_DB", ROOT / "data" / "aeroos.sqlite3"))

@contextmanager
def connect():
    DB.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(DB)
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA foreign_keys=ON")
    try:
        with db:
            yield db
    finally:
        db.close()

def init():
    with connect() as db:
        db.executescript("""
        CREATE TABLE IF NOT EXISTS scenarios(id INTEGER PRIMARY KEY, name TEXT NOT NULL, payload TEXT NOT NULL, created TEXT DEFAULT CURRENT_TIMESTAMP);
        CREATE TABLE IF NOT EXISTS runs(id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind TEXT NOT NULL, input TEXT NOT NULL, output TEXT NOT NULL, created TEXT DEFAULT CURRENT_TIMESTAMP);
        CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY, run_id INTEGER REFERENCES runs(id) ON DELETE CASCADE, time INTEGER, payload TEXT);
        CREATE TABLE IF NOT EXISTS metrics(run_id INTEGER REFERENCES runs(id) ON DELETE CASCADE, payload TEXT);
        CREATE TABLE IF NOT EXISTS snapshots(id INTEGER PRIMARY KEY, name TEXT NOT NULL, run_id INTEGER REFERENCES runs(id), position INTEGER NOT NULL, created TEXT DEFAULT CURRENT_TIMESTAMP);
        """)

def save_run(name, kind, inputs, output):
    with connect() as db:
        rid = db.execute("INSERT INTO runs(name,kind,input,output) VALUES(?,?,?,?)", (name, kind, json.dumps(inputs), json.dumps(output))).lastrowid
        db.execute("INSERT INTO metrics VALUES(?,?)", (rid, json.dumps(output.get("metrics", {}))))
        db.executemany("INSERT INTO events(run_id,time,payload) VALUES(?,?,?)", [(rid, s["time"], json.dumps(e)) for s in output.get("snapshots", []) for e in s["events"]])
    return rid
