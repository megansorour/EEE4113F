from flask import Flask, render_template

app = Flask(__name__)

weight = 100  # Replace with your actual weight variable

@app.route('/')
def index():
    return render_template('index.html', weight=weight)

@app.route('/update_weight', methods=['POST'])
def update_weight():
    global weight
    weight += 1  # Update the weight
    return render_template('index.html', weight=weight)

if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True)