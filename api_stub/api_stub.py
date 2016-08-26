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


def produce_plot(mean_window):
    if not mean_window:
        mean_window = 20

    timestamps, moistures, voltages = [], {'moisture': []}, {'voltage': []}
    for data_point in data_store.find():

        moisture = float(data_point.get('soil moisture', 0))
        moistures['moisture'].append(moisture)
        avg_moist_df = pd.DataFrame(moistures)
        avg_moist = avg_moist_df.rolling(center=False,
                                         window=mean_window).mean()

        voltage = float(data_point.get('voltage', 0))
        voltages['voltage'].append(voltage)
        avg_voltage_df = pd.DataFrame(voltages)
        avg_voltage = avg_voltage_df.rolling(center=False,
                                             window=mean_window).mean()

        # UTC -> PDT, server is 3 hours fast
        timestamp = data_point.get('timestamp') - 43200
        timestamps.append(timestamp * 1000)

    output_file(config.bokeh_output_dir + 'lines.html')
    p = figure(title='ESP8266 Sensor #1',
               x_axis_label='time (UTC-8:00)',
               x_axis_type='datetime',
               y_axis_label='voltage',
               tools='',
               y_range=(3,4))

    p.line(timestamps,
           avg_voltage,
           legend="voltage",
           line_width=2,
           color='red')

    # moisture
    p.extra_y_ranges = {'moisture': Range1d(start=-5, end=100)}
    p.line(timestamps,
           avg_moist,
           legend="moisture",
           line_width=2,
           color='blue',
           y_range_name='moisture')
    p.add_layout(LinearAxis(y_range_name='moisture',
                            axis_label='moisture'),
                 'right')

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

        r = data_store.insert_one(data_point)
        print('MONGO ID:', r.inserted_id)
        
        produce_plot()
        
        # TODO: return sensor data hash
        return str(data_point['timestamp'])

    elif request.method == 'GET':
        print('\n\nGET')
        return 'GET received'


@app.route('/api', methods=['GET'])
def display():
    print('\n\nGET')
    if request.args.get('refresh'):
        print('refreshing plot')
        produce_plot(int(request.args.get('window')))
    return send_from_directory(config.static_files_path,
                               config.bokeh_output_dir + 'lines.html')


@app.route('/gap_test', methods=['GET'])
def gap_test():
    from math import pi

    import pandas as pd

    from bokeh.plotting import figure, show, output_file
    from bokeh.sampledata.stocks import MSFT

    df = pd.DataFrame(MSFT)[:50]
    df["date"] = pd.to_datetime(df["date"])

    mids = (df.open + df.close)/2
    spans = abs(df.close-df.open)

    inc = df.close > df.open
    dec = df.open > df.close
    w = 12*60*60*1000 # half day in ms

    TOOLS = "pan,wheel_zoom,box_zoom,reset,save"

    p = figure(x_axis_type="datetime", tools=TOOLS, plot_width=1000, title = "MSFT Candlestick")
    p.xaxis.major_label_orientation = pi/4
    p.grid.grid_line_alpha=0.3

    p.segment(df.date, df.high, df.date, df.low, color="black")
    p.rect(df.date[inc], mids[inc], w, spans[inc], fill_color="#D5E1DD", line_color="black")
    p.rect(df.date[dec], mids[dec], w, spans[dec], fill_color="#F2583E", line_color="black")

    output_file(config.bokeh_output_dir + 'candlestick.html', title='candlestick.py example')

    save(p)
    return send_from_directory(config.static_files_path,
                               config.bokeh_output_dir + 'candlestick.html')


if __name__ == "__main__":
    # http2 instead of websockets
    app.run(host='0.0.0.0', port=config.flask_port)
