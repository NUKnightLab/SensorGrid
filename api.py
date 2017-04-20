from flask import Flask, request
app = Flask(__name__)
import datetime
import sys
from struct import *

history = []

PROTOCOL_VERSIONS = {
    '0.11': '<HHHHIHxxI10i'
}
PROTOCOL = PROTOCOL_VERSIONS['0.11']


@app.route('/')
def main():
    return 'Knight Lab SensorGrid'


@app.route('/data', methods=['POST'])
def data():
    # we should be checking data length. See Flask docs:
    # http://werkzeug.pocoo.org/docs/0.11/wrappers/#werkzeug.wrappers.BaseRequest.get_data
    if request.json.get('ver') == '0.11':
        # json encoded message
        msg = request.json
        ( ver, net, snd, orig, msg_id, bat, timestamp, data) = (
            float(msg['ver']), int(msg['net']), int(msg['snd']),
            int(msg['orig']), int(msg['id']),
            float(msg['bat']), int(msg['timestamp']),
            [int(d) for d in msg['data']])
    else:
        # raw struct message
        msg = request.get_data(cache=False)
        ver_100, net, snd, orig, msg_id, bat_100, timestamp, \
            *data = unpack(PROTOCOL, msg)
        ver = ver_100 / 100.0
        bat = bat_100 / 100.0
    sys.stdout.flush()
    global history
    history.append({
        'dt': datetime.datetime.now(),
        'orig': orig,
        'bat': bat,
        'timestamp': timestamp,
        'datetime': str(datetime.datetime.fromtimestamp(timestamp)),
        'data': str(data)
    })
    history = history[-30:]
    return 'OK'


@app.route('/report')
def report():
    return '<br/>'.join(
        ['%s:: %s %sv %s %s %s' % (r['dt'], r['orig'], r['bat'], r['timestamp'], r['datetime'], r['data']) for r in reversed(history)])


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=9022)
