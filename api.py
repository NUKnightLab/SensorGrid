from flask import Flask, request
app = Flask(__name__)
import datetime
import sys
from struct import *

history = []

PROTOCOL_VERSIONS = {
    '0.11': '<HHHHIHIBBBBBB?xxxiiBxxx10i'
}
PROTOCOL = PROTOCOL_VERSIONS['0.11']


@app.route('/')
def main():
    return 'Knight Lab SensorGrid'


@app.route('/data', methods=['POST'])
def data():
    # we should be checking data length. See Flask docs:
    # http://werkzeug.pocoo.org/docs/0.11/wrappers/#werkzeug.wrappers.BaseRequest.get_data
    message = request.get_data(cache=False)
    #print('-'*30)
    #print("REC'd message len: %d" % len(message))
    ver_100, net, snd, orig, msg_id, bat_100, timestamp, year, month, day, hour, minute, seconds, \
        fix, lat_1000, lon_1000, sats, \
        *data = unpack(PROTOCOL, message)
    ver = ver_100 / 100.0
    bat = bat_100 / 100.0
    lat = lat_1000 / 1000.0
    lon = lon_1000 / 1000.0
    #print((ver, net, snd, orig, msg_id, bat))
    #print('%d-%d-%dT%d:%d:%d' % (year, month, day, hour, minute, seconds))
    #print((fix, lat, lon, sats))
    #print(data)
    sys.stdout.flush()
    global history
    history.append({
        'dt': datetime.datetime.now(),
        'orig': orig,
        'bat': bat,
        'datetime': str(datetime.datetime.fromtimestamp(timestamp)),
        'data': str(data)
    })
    history = history[-30:]
    return 'OK'


@app.route('/report')
def report():
    return '<br/>'.join(
        ['%s:: %s %sv %s %s' % (r['dt'], r['orig'], r['bat'], r['datetime'], r['data']) for r in reversed(history)])


if __name__ == '__main__':
    app.run(host='0.0.0.0')
