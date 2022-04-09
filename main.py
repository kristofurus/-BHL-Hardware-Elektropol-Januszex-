import json

from flask import Flask, render_template, request
import requests

app = Flask(__name__)
hr = 0
time = ""


@app.route('/')
def home():
    return render_template("index.html")


@app.route('/data', methods=["PUT", "POST"])
def get_data():
    global hr, time
    if request.method == 'POST':
        print(request)
        print(request.headers)
        if request.is_json:
            data = request.json
            print(data)
            hr = data["hr"]
            time = data["time"]
        return 'JSON posted'


@app.route('/arduino')
def arduino():
    # response = requests.get("http://192.168.172.203/data")
    # response = requests.get("https://google.com/")
    # data = response.text
    # hr_value = data["hr"]
    # fall_detected = data["time"]
    # print(data)
    # hr_value = 10
    # fall_detected = False
    return render_template("status.html", hr_value=hr, fall_detected=time)


@app.route('/configuration')
def notify():
    return render_template('notification_menu.html')

@app.route('/notification_menu')
def set_notification():
    return render_template('set_notification.html')


if __name__ == "__main__":
    app.run(debug=True, host="192.168.45.190", port=8080)
