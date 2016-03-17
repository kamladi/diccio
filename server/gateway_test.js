var SP         = require('serialport');
var SerialPort = SP.SerialPort;
var Outlet     = require('./models/Outlet');
var Event      = require('./models/Event');

// Constants
const DEFAULT_SERIAL_PORT = '/dev/tty.usbserial-AE00BUO5';
const BAUD_RATE = 115200;
const OUTLET_SENSOR_MESSAGE = 0;
const OUTLET_ACTION_MESSAGE = 6;

// Globals
var serialPort = null;
var hasStarted = false;
var seqNum = 1;

// Parse and handle data packet.
// TODO:
// 1) Update time series sensor data
// 2) Iterate over events involving this outlet, and
// 			execute any actions is applicable
function handleData(data) {
  console.log("RX: ", data);

	/** Parse Packet **/
	/** Packet format: "mac_addr:seq_num:msg_id:payload" **/
	var components = data.split(':');
	if (components.length !== 4) {
		console.trace("Invalid minimum packet length");
		return;
	}
	var macAddress = components[0],
	    msgId = components[2],
	    payload = components[3];
	return Outlet.findOne({mac_address: macAddress}).exec()
	  .then( outlet => {
	    if (!outlet) {
	      throw new Error(`Outlet does not exist for MAC Address: ${macAddress}`);
	    }
	    if (msgId !== OUTLET_SENSOR_MESSAGE) {
	      throw new Error(`Invalid Message ID: ${msgId}`);
	    }

	    // Parse sensor data, convert to ints
	    var sensorValues = payload.split(',').map(value => parseInt(value));

	    // We're expecting five values: status, temp, humidity, light, and power.
	    if (sensorValues.length !== 5) {
	      throw new Error(`Not enough sensor values in packet: ${sensorValues}`);
	    }
	    var status = sensorValues[0],
	        cur_temperature = sensorValues[1],
	        cur_humidity = sensorValues[2],
	        cur_light = sensorValues[3],
	        cur_power = sensorValues[4];

	    // Update outlet object with new properties, and save.
	    outlet.status = status;
	    outlet.cur_temperature = cur_temperature;
	    outlet.cur_humidity = cur_humidity;
	    outlet.cur_light = cur_light;
	    outlet.cur_power = cur_power;
	    return outlet.save();
	  }).catch(console.trace);
}

/*
 * Given an outlet's mac address and an action ('ON'/'OFF'),
 * Send a message to the gateway to be propagated to that outlet.
 * Returns a promise which will be fulfilled when the packet is sent.
 * Packet format: "source_mac_addr:seq_num:msg_type:num_hops:payload"
 * Where "payload" has structure "dest_outlet_id,action,"
 */
function sendAction(outletMacAddress, action) {
  return new Promise( (resolve, reject) => {
    if (!hasStarted) {
      reject(new Error("Connection to gateway has not started yet"));
    } else if (action !== 'ON' && action !== 'OFF') {
      reject(new Error(`Invalid action: ${action}. Must be "ON" or "OFF"`));
    } else {
      // Convert action string to enum value
      action = (action === 'ON') ? 1 : 0;
      // server sends message with source_id 0, seq_num 0, num_hops 0
      var packet = `0:0:${OUTLET_ACTION_MESSAGE}:0:0,${outletMacAddress},${action},`;
      packet += "\r";
      console.log("packet sent is..." + packet);
      serialPort.write(packet, (err) => {
        if (err) {
          reject(err);
        } else {
	      	serialPort.drain((err) => {
	      		if(err){
	      			reject(err);
	      		} else {
	      			resolve(packet);
	      		}
	     		});
        }
      });
    }
  });
};

/*
 * Starts the connection to the gateway node. If a port was not given as an
 * argument, it assumed a constant defined above
 */
function start(port, callback) {
	if (!port) {
		port = DEFAULT_SERIAL_PORT;
	}
	// Init serial port connection
	serialPort = new SerialPort(port, {
	    baudRate: BAUD_RATE,
	    parser: SP.parsers.readline("\r")
	});

	// Listen for "open" event form serial port
	serialPort.on('open', () => {
	    hasStarted = true;
	    console.log('Serial Port opened');

	    // Listen for "data" event from serial port
	    serialPort.on('data', (data) => console.log("received: ", data));

	    callback(null);
	});

	serialPort.on('error', (err) => {
		hasStarted = false;
		console.error('Serial Port Error: ', err);
		callback(err);
	});
};

// Get serial port from command line args
var port = DEFAULT_SERIAL_PORT;
if (process.argv.length >= 3) {
	port = process.argv[2];
}

start(port, function(err) {
	if (err) {
		console.error(err);
	}
	
	setInterval(function() {
		sendAction(1,"ON").catch(console.trace);
	},2000);
});