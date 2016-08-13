from flask import Flask, request

import config

app = Flask(__name__)


@app.route('/', methods=['GET', 'POST'])
def barf():
    if request.method == 'POST':
        #print(request.data)
        print('\n\nsoil moisture: {}\nvoltage: {}'.format(
              request.form['soil_moisture'],
              request.form['bus_voltage']))
        return 'hello world'
    elif request.method == 'GET':
        print('get')
        return 'get received'


if __name__ == "__main__":
    app.run(host='0.0.0.0', port=config.flask_port)
