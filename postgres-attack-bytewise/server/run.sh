#!/bin/bash

set -e

export FLASK_APP=app.py
taskset -c 3 flask run --host=0.0.0.0 --port=8080