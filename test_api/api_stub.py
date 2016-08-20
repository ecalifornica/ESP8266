import time

from flask import Flask, request
import pymongo

import config

mc = pymongo.MongoClient()
data_store = mc.sensornet.sensors

app = Flask(__name__)


@app.route('/', methods=['GET', 'POST'])
def barf():
    if request.method == 'POST':
        print('\n\nPOST')
        # print(request.data)
        soil_moisture = request.form['soil_moisture']
        voltage = request.form['bus_voltage']
        data_point = {'soil moisture': soil_moisture,
                      'voltage': voltage,
                      'serial_number': 1,
                      'timestamp': time.mktime(time.gmtime())}
        import pprint
        pprint.pprint(data_point)
        data_store.insert_one(data_point)
        return str(data_point['timestamp'])
    elif request.method == 'GET':
        print('\n\nGET')
        return 'get received'


@app.route('/api', methods=['GET'])
def display():
    print('PLOT GET')
    plot_data = 'TIMESTAMP,SERIAL,DATA\n'
    for data_point in data_store.find():
        plot_data += '{},{},{}\n'.format(data_point.get('timestamp'),
                                         data_point.get('serial_number'),
                                         data_point.get('voltage'))
    return plot_data


if __name__ == "__main__":
    app.run(host='0.0.0.0', port=config.flask_port)
