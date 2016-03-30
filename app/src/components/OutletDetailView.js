'use strict';
import React, {
  AppRegistry,
  Component,
  NavigatorIOS,
  ListView,
  StyleSheet,
  Text,
  View,
  TextInput,
  TouchableHighlight,
  AlertIOS
} from 'react-native';
import RNChart from 'react-native-chart';

import {EditableTextField} from './EditableTextField';
import {API_OUTLETS_URL} from '../lib/Constants';
import OutletStore from '../stores/OutletStore';
import OutletActions from '../actions/OutletActions';

class OutletChart extends Component {
  constructor(props) {
    super(props);
    this.state = {
      xLabels: [],
      data: []
    };
  }

  componentDidMount() {
    var self = this;
    fetch('http://localhost:3000/graphs/' + this.props.outlet_id)
      .then((response) => response.json())
      .then( graphData => {
        console.log(graphData);
        self.setState({
          xLabels: graphData.slice(0,10).map(record => new Date(record.timestamp).getSeconds()),
          data: graphData.slice(0,10).map(record => record.power)
        });
        console.log(self.state);
      }).catch(console.errror);
  }

  render() {
    var chartData = [{
      name: 'LineChart',
      color: 'gray',
      lineWidth: 2,
      highlightIndices: [1, 2],   // The data points at indexes 1 and 2 will be orange
      highlightColor: 'orange',
      data: this.state.data,
      showDataPoint: true
    }];

    return (
      /*<RNChart style={styles.chart}
          chartData={chartData}
          verticalGridStep={5}
          xLabels={this.state.xLabels}
        />*/
      <Text>Chart Placeholder</Text>
    );
  }
}

class OutletActionButton extends Component {
	constructor(props) {
		super(props);
		this.onButtonPressed = this.onButtonPressed.bind(this);
	}

	onButtonPressed() {
    // toggle the current state to get the action we want to send.
    var action = (this.props.status === 'ON') ? 'off' : 'on';
		fetch(`${API_OUTLETS_URL}/${this.props.outlet_id}/${action}`)
      .then((response) => response.json())
      .then((responseData) => {
        this.setState({
          outlet: responseData,
          loaded: true,
        });
      })
      .then( () => AlertIOS.alert(`"${action.toUpperCase()}" command sent to outlet.`))
      .done();
	}

	render() {
		if (this.props.status === 'ON') {
			return (
				<TouchableHighlight
					style={[styles.button, styles.buttonOff]}
          underlayColor='#CCCCCC'
					onPress={() => this.onButtonPressed()}>
					<Text style={styles.buttonText}>Turn Off</Text>
				</TouchableHighlight>
			)
		} else {
			return (
				<TouchableHighlight
					style={[styles.button, styles.buttonOn]}
          underlayColor='#CCCCCC'
					onPress={() => this.onButtonPressed()}>
					<Text style={styles.buttonText}>Turn On</Text>
				</TouchableHighlight>
			)
		}

	}
}

export class OutletDetailView extends Component {
	constructor(props) {
		super(props);
		this.state = {
      outlet: null,
      loaded: false
    };
    this.onChange = this.onChange.bind(this);
    this.onNameChange = this.onNameChange.bind(this);
	}

	componentDidMount() {
    this.setState({
      outlet: OutletStore.getState().outlets.filter( (outlet) => outlet._id === this.props.outlet_id)[0],
      loaded: true
    });
    OutletStore.listen(this.onChange);
    OutletActions.fetchOutlet(this.props.outlet_id);

    // Regularly ping the server for the latest outlet data.
    this.updateIntervalId = setInterval(() => {
      OutletActions.fetchOutlet(this.props.outlet_id);
    }, 500);
  }

  componentWillUnmount() {
    OutletStore.unlisten(this.onChange);
    clearInterval(this.updateIntervalId);
  }

  onChange(state) {
    this.setState({
      outlet: state.outlets.filter( (outlet) => outlet._id === this.props.outlet_id)[0],
      loaded: true
    });
  }

  onNameChange(newName) {
    var name = newName.trim();
    if (name.length === 0) {
      AlertIOS.alert("Invalid outlet name");
      return false;
    }
    OutletActions.updateOutletName(this.props.outlet_id, name);
    return true;
  }

  getGraphData(granularity) {
    return fetch()
  }


  renderLoadingView() {
    return (
      <View style={styles.container}>
        <Text>
          Loading outlets...
        </Text>
      </View>
    );
  }
	render() {
		if (!this.state.loaded) {
			return this.renderLoadingView();
		}

		var outlet = this.state.outlet;
		return (
			<View style={styles.container}>
        <View style={styles.row}>
				  <Text style={styles.label}>Change name:</Text>
          <EditableTextField value={outlet.name} onSubmit={this.onNameChange} />
				</View>
        <View style={styles.row}>
          <Text style={styles.label}>Temp:</Text>
          <Text style={styles.sensorValue}>{outlet.cur_temperature}</Text>
        </View>
        <View style={styles.row}>
				  <Text style={styles.label}>Humidity:</Text>
          <Text style={styles.sensorValue}>{outlet.cur_humidity}</Text>
        </View>
        <View style={styles.row}>
				  <Text style={styles.label}>Light:</Text>
          <Text style={styles.sensorValue}>{outlet.cur_light}</Text>
        </View>
        <View style={styles.row}>
				  <Text style={styles.label}>Power:</Text>
          <Text style={styles.sensorValue}>{outlet.cur_power}</Text>
        </View>
        <View style={styles.row}>
				  <Text style={styles.label}>Status:</Text>
          <Text style={styles.sensorValue}>{outlet.status}</Text>
          <OutletActionButton status={outlet.status} outlet_id={outlet._id} />
        </View>
        <View style={styles.row}>
          <OutletChart outlet_id={outlet._id} />
        </View>
			</View>
		);
	}
}

const styles = StyleSheet.create({
	container: {
    flex: 1,
    marginTop: 65,
    backgroundColor: '#EEEEEE',
    alignItems: 'center'
  },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    marginTop: 15
  },
  label: {
    fontSize: 20,
    marginRight: 15,
    flex: 1
  },
  sensorValue: {
    fontSize: 30,
    fontWeight: 'bold',
    flex: 1
  },
  buttonText: {
  	fontSize: 20,
    color: '#FFFFFF'
  },
  button: {
    marginTop: 15,
    marginBottom: 15,
    marginLeft: 10,
  	padding: 10,
  	borderRadius: 10
  },
  buttonOn: {
  	backgroundColor: 'green',
  },
  buttonOff: {
  	backgroundColor: 'red',
  },
  refreshButton: {
    backgroundColor: '#51C6ED'
  },
  chart: {
    marginLeft: 10,
    marginRight: 10,
  }
});
