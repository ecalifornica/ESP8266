import time

from bokeh.plotting import figure, output_file, save
from bokeh.models import LinearAxis, Range1d
from flask import Flask, request, send_from_directory
import pandas as pd
import pymongo

import config

mc = pymongo.MongoClient()
data_store = mc.sensornet.sensors

app = Flask(__name__)


def running_average():
    _sum = 0.0
    count = 0
    value = yield(float('nan'))
    while True:
        _sum += value
        count += 1
        value = yield(_sum/count)

ravg = running_average()
next(ravg)


def produce_plot():
    timestamps, moistures, voltages = [], {'moisture': []}, {'voltage': []}
    for data_point in data_store.find():
        moisture = float(data_point.get('moisture', 0))
        moistures['moisture'].append(moisture)
        avg_moist_df = pd.DataFrame(moistures)
        avg_moist = avg_moist_df.rolling(center=False,
                                         window=30).mean()

        voltage = float(data_point.get('voltage', 0))
        voltages['voltage'].append(voltage)
        avg_voltage_df = pd.DataFrame(voltages)
        avg_voltage = avg_voltage_df.rolling(center=False,
                                             window=20).mean()
        # avg_v = ravg.send(voltage)
        # voltages.append(avg_v)

        # timestamps.append(data_point.get('timestamp') * 1000)
        timestamp = data_point.get('timestamp') - 36000
        timestamps.append(timestamp * 1000)

    output_file('test_api/bokeh_output/lines.html')
    p = figure(title='ESP8266 Sensor #1',
               x_axis_label='time (UTC-7:00)',
               x_axis_type='datetime',
               y_axis_label='voltage',
               tools='')

    p.extra_y_ranges = {'barf': Range1d(start=-5, end=100)}
    p.extra_y_ranges = {'voltage': Range1d(start=-5, end=100)}

    p.line(timestamps,
           avg_voltage,
           legend="v",
           line_width=2,
           color='red')
    '''
    p.line(timestamps,
           avg_moist,
           legend="moisture",
           line_width=2,
           color='blue',
           y_range_name='barf')

    p.add_layout(LinearAxis(y_range_name = 'barf',
                            axis_label='moisture'),
                            'right')

    '''
    save(p)


@app.route('/', methods=['GET', 'POST'])
def barf():
    if request.method == 'POST':
        print('\n\nPOST')
        # print(request.data)
        soil_moisture = float(request.form['soil_moisture'])
        voltage = float(request.form['bus_voltage'])
        timestamp = time.gmtime()
        data_point = {'soil moisture': soil_moisture,
                      'voltage': voltage,
                      'serial_number': 1,
                      'timestamp': time.mktime(timestamp)}
        print('{} {}% {}v'.format(time.strftime('%Y-%m-%d %H:%M:%S',
                                                timestamp),
                                  soil_moisture,
                                  voltage))
        data_store.insert_one(data_point)

        produce_plot()

        return str(data_point['timestamp'])

    elif request.method == 'GET':
        print('\n\nGET')

        return send_from_directory(config.static_files_path,
                                   'test_api/bokeh_output/lines.html')


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
    # http2 instead of websockets
    app.run(host='0.0.0.0', port=int(config.flask_port), debug=True)
