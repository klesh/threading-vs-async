import time
import redis
from flask import Flask
from threading import Lock
import logging
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

app = Flask(__name__)

lock = Lock()
counter = 1

@app.route('/')
def index():
    global counter
    lock.acquire()
    counter += 1
    lock.release()
    return str(counter)

@app.route('/slow')
def slow():
    time.sleep(1)
    return 'ok'
