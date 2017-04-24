import flask
from flask import Flask, request
app = Flask(__name__)
import datetime
import pymongo
import os
import sys
from struct import *

history = []
MONGO_DB_URI = os.environ.get('MONGO_DB_URI', 'mongodb://localhost:27017/')
db = pymongo.MongoClient(MONGO_DB_URI).sensorgrid

PROTOCOL_VERSIONS = {
    '0.11': '<HHHHIHxxI10i'
}
PROTOCOL = PROTOCOL_VERSIONS['0.11']


@app.route('/')
def main():
    return 'Knight Lab SensorGrid'


def convert_mongo_id(record):
    record['record_id'] = str(record['_id'])
    del record['_id']
    return record


@app.route('/data', methods=['GET', 'POST'])
def data():
    if request.method == 'GET':
        return flask.json.jsonify(
            list(map(convert_mongo_id, db.data.find(
                limit=100)\
                .sort('created_at', pymongo.DESCENDING))))
    # we should be checking data length. See Flask docs:
    # http://werkzeug.pocoo.org/docs/0.11/wrappers/#werkzeug.wrappers.BaseRequest.get_data
    if request.json.get('ver') == '0.11': # json encoded message
        msg = request.json
        ( ver, net, snd, orig, msg_id, bat, timestamp, data) = (
            float(msg['ver']), int(msg['net']), int(msg['snd']),
            int(msg['orig']), int(msg['id']),
            float(msg['bat']), int(msg['timestamp']),
            [int(d) for d in msg['data']])
    else: # raw struct message
        msg = request.get_data(cache=False)
        ver_100, net, snd, orig, msg_id, bat_100, timestamp, \
            *data = unpack(PROTOCOL, msg)
        ver = ver_100 / 100.0
        bat = bat_100 / 100.0
    sys.stdout.flush()
    global history
    record = {
        'version': ver,
        'network': net,
        'snd': snd,
        'message_id': msg_id,
        'received_at': datetime.datetime.now(),
        'node_id': orig,
        'battery': bat,
        'timestamp': timestamp,
        'created_at': datetime.datetime.fromtimestamp(timestamp),
        'data': [ int(i) for i in data ]
    }
    history.append(record)
    history = history[-30:]
    db.data.insert_one(record)
    return 'OK'


@app.route('/report')
def report():
    return '<br/>'.join(
        ['%s:: %s %sv %s %s %s' % (r['dt'], r['orig'], r['bat'],
        r['timestamp'], r['datetime'], r['data']) for r in reversed(history)])


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=9022)
