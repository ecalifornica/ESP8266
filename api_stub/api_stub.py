import logging
import time

from bokeh.plotting import figure, output_file, save
from bokeh.models import LinearAxis, Range1d
from flask import Flask, request, send_from_directory
import pandas as pd
import pymongo
from raven.contrib.flask import Sentry

import config

logger = logging.getLogger('api_stub')
logger.setLevel(logging.DEBUG)
stdo = logging.StreamHandler()
logger.addHandler(stdo)

application = Flask(__name__)

sentry = Sentry(application, logging=True, level=logging.ERROR,
                dsn=config.sentry_dsn)

mc = pymongo.MongoClient()
data_store = mc.sensornet.sensors


def produce_plot(mean_window=None, search_limit=None):
    '''
    '''
    if not mean_window:
        mean_window = 1
    else:
        mean_window = int(mean_window)

    if not search_limit:
        search_limit = 1000
    else:
        search_limit = int(search_limit)

    try:
        timestamps, moistures, voltages = [], {'moisture': []}, {'voltage': []}
        for data_point in data_store.find(sort=[('timestamp', -1)],
                                          limit=search_limit):
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
                   y_range=(3, 4.3))

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
    except:
        sentry.captureMessage('produce plot failure')
        sentry.captureException()


@application.route('/', methods=['GET', 'POST'])
def barf():

    if request.method == 'POST':
        try:
            logger.info('\n\nPOST')
            logger.info(request)
            # print(request.data)
            soil_moisture = float(request.form['soil_moisture'])
            voltage = float(request.form['bus_voltage'])
            timestamp = time.gmtime()
            data_point = {'soil moisture': soil_moisture,
                          'voltage': voltage,
                          'serial_number': 1,
                          'timestamp': time.mktime(timestamp)}
            logger.info('{} {}% {}v'.format(time.strftime('%Y-%m-%d %H:%M:%S',
                                                          timestamp),
                                            soil_moisture,
                                            voltage))

            r = data_store.insert_one(data_point)
            logger.info('MONGO ID:', r.inserted_id)

            produce_plot()

            # TODO: return sensor data hash
            return str(data_point['timestamp'])
        except:
            sentry.captureMessage('sensor data POST failure')
            sentry.captureException()

    elif request.method == 'GET':
        try:
            logger.info('\n\nGET')
            logger.info(request)
            return 'GET received'
        except:
            sentry.captureMessage('sensor GET (pre post) failure')
            sentry.captureException()


@application.route('/api', methods=['GET'])
def display():
    logger.info('\n\nAPI GET: {}'.format(time.strftime('%Y-%m-%d %H:%M:%S',
                                                       time.gmtime())))
    refresh = request.args.get('refresh')
    window = request.args.get('window')
    limit = request.args.get('limit')
    if refresh:
        logger.info('refreshing plot')
        produce_plot(mean_window=window, search_limit=limit)
    return send_from_directory(config.static_files_path,
                               config.bokeh_output_dir + 'lines.html')


@application.route('/gap_test', methods=['GET'])
def gap_test():
    from math import pi

    import pandas as pd

    from bokeh.plotting import figure, save, output_file
    from bokeh.sampledata.stocks import MSFT

    df = pd.DataFrame(MSFT)[:50]
    df["date"] = pd.to_datetime(df["date"])

    mids = (df.open + df.close)/2
    spans = abs(df.close-df.open)

    inc = df.close > df.open
    dec = df.open > df.close
    w = 12*60*60*1000  # half day in ms

    TOOLS = "pan,wheel_zoom,box_zoom,reset,save"

    p = figure(x_axis_type="datetime", tools=TOOLS, plot_width=1000, title="MSFT Candlestick")
    p.xaxis.major_label_orientation = pi/4
    p.grid.grid_line_alpha = 0.3

    p.segment(df.date, df.high, df.date, df.low, color="black")
    p.rect(df.date[inc], mids[inc], w, spans[inc], fill_color="#D5E1DD", line_color="black")
    p.rect(df.date[dec], mids[dec], w, spans[dec], fill_color="#F2583E", line_color="black")

    output_file(config.bokeh_output_dir + 'candlestick.html', title='candlestick.py example')

    save(p)
    return send_from_directory(config.static_files_path,
                               config.bokeh_output_dir + 'candlestick.html')


if __name__ == "__main__":
    # http2 instead of websockets
    '''
    print('Loggers:')
    for i in logging.Logger.manager.loggerDict:
        print(i)
    '''
    logger.info('Running server...')
    application.run(host='0.0.0.0', port=config.flask_port)