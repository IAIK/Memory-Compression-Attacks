from flask import Flask, request
import rdtsc
import pandas as pd
import psycopg2
from psycopg2.extensions import ISOLATION_LEVEL_AUTOCOMMIT

RUNS = 1000
DICT_SIZE = 5

app = Flask(__name__)
conn = None

def setup_db():
    global conn
    # Connect to your postgres DB
    conn = psycopg2.connect("user=postgres password=postgres",host="127.0.0.1", port="5432")
    conn.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)

    # Open a cursor to perform database operations
    cur = conn.cursor()

    cur.execute("SELECT 1 FROM pg_catalog.pg_database WHERE datname = 'testdb'")
    exists = cur.fetchone()
    if not exists:
        cur.execute('CREATE DATABASE testdb')
    conn.close()
    cur.close()

    # Connect to your postgres DB
    conn = psycopg2.connect("dbname=testdb user=postgres password=postgres",host="127.0.0.1",port="5432")
    conn.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)

    # Open a cursor to perform database operations

    # Execute a query
    with conn.cursor() as cur:
        cur.execute("DROP TABLE IF EXISTS Test")
        cur.execute("CREATE TABLE Test(Id INTEGER PRIMARY KEY, Data TEXT)")

    print("DATABASE INITIALIZED")

@app.route('/')
def hello_world():
    return 'Hello, World!'

@app.route('/reset')
def reset():
    setup_db()
    return 'reset'

@app.route('/get_id')
def get():
    id = request.args.get('id')
    with conn.cursor() as cur:
        start = rdtsc.get_cycles()
        cur.execute("SELECT Data FROM Test WHERE Id=%s", [id])
        end = rdtsc.get_cycles()

        # Retrieve query results
        # records = cur.fetchone()
    # if records is None:
    #     return "ERROR"
    # return records[0]
    return str(end - start)
    # return ''

@app.route('/post_id', methods=['POST'])
def post():
    id = request.form.get('id')
    data = request.form.get('data')

    # Execute a query
    with conn.cursor() as cur:
        cur.execute("INSERT INTO Test (Id, Data) VALUES (%s, %s)", [id, data])
        cur.execute("SELECT sum(pg_column_size(Data)) FROM Test WHERE Id=%s", [id])
        records = cur.fetchall()
        size = records[0][0]

    return str(size)


@app.route('/autotest')
def autotest():
    columns = ["Secret", "Delta"]
    data = []
    for i in range(RUNS):
        for i in range(DICT_SIZE):
            with conn.cursor() as cur:
                start = rdtsc.get_cycles()
                cur.execute("SELECT Data FROM Test WHERE Id=%s", [i])
                end = rdtsc.get_cycles()
                data.append([i, end-start])
    df = pd.DataFrame(data=data, columns=columns)
    return df.groupby(["Secret"])['Delta'].median().to_markdown().replace('\n', '</br>')

setup_db()
