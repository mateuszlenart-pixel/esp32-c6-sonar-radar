import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as fz from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as tz from 'zigbee-herdsman-converters/converters/toZigbee';

const access = exposes.access;
const e = exposes.presets;

const radarMeasurements = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        if (msg.endpoint.ID === 3 && msg.data.presentValue !== undefined) {
            return {scan_angle: msg.data.presentValue};
        }
        if (msg.endpoint.ID === 4) {
            const result = {};
            if (msg.data.presentValue !== undefined) result.distance = msg.data.presentValue;
            if (msg.data.outOfService !== undefined) result.distance_valid = !msg.data.outOfService;
            if (Object.keys(result).length > 0) return result;
        }
    },
};

const radarOccupancy = {
    cluster: 'msOccupancySensing',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        if (msg.data.occupancy === undefined) return;
        const detected = (msg.data.occupancy & 1) === 1;
        if (msg.endpoint.ID === 1) return {target_detected: detected};
        if (msg.endpoint.ID === 2) return {critical: detected};
    },
};

const radarThresholds = {
    cluster: 'genAnalogOutput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        if (msg.data.presentValue === undefined) return;
        if (msg.endpoint.ID === 7) return {target_threshold: msg.data.presentValue};
        if (msg.endpoint.ID === 8) return {critical_threshold: msg.data.presentValue};
    },
};

const setRadarThreshold = {
    key: ['target_threshold', 'critical_threshold'],
    convertSet: async (entity, key, value, meta) => {
        const endpointId = key === 'target_threshold' ? 7 : 8;
        const endpoint = meta.device.getEndpoint(endpointId);
        const numericValue = Number(value);
        await endpoint.write('genAnalogOutput', {presentValue: numericValue});
        return {state: {[key]: numericValue}};
    },
    convertGet: async (entity, key, meta) => {
        const endpointId = key === 'target_threshold' ? 7 : 8;
        await meta.device.getEndpoint(endpointId).read('genAnalogOutput', ['presentValue']);
    },
};

export default {
    zigbeeModel: ['ESP32C6-SonarRadar'],
    model: 'ESP32C6-SonarRadar',
    vendor: 'Mateusz Lenart',
    description: 'ESP32-C6 sonar radar with Zigbee and MQTT',
    fromZigbee: [radarMeasurements, radarOccupancy, radarThresholds, fz.on_off],
    toZigbee: [tz.on_off, setRadarThreshold],
    exposes: [
        e.binary('target_detected', access.STATE, true, false),
        e.binary('critical', access.STATE, true, false),
        e.numeric('distance', access.STATE).withUnit('cm'),
        e.binary('distance_valid', access.STATE, true, false),
        e.numeric('scan_angle', access.STATE).withUnit('°')
            .withHomeAssistant({icon: 'mdi:angle-acute'}),
        e.numeric('target_threshold', access.STATE_SET).withUnit('cm')
            .withValueMin(10).withValueMax(400).withValueStep(1),
        e.numeric('critical_threshold', access.STATE_SET).withUnit('cm')
            .withValueMin(5).withValueMax(300).withValueStep(1),
        e.switch().withEndpoint('patrol'),
        e.switch().withEndpoint('buzzer'),
    ],
    endpoint: () => ({patrol: 5, buzzer: 6}),
    meta: {multiEndpoint: true},
    configure: async (device, coordinatorEndpoint) => {
        for (const id of [1, 2]) {
            const endpoint = device.getEndpoint(id);
            await endpoint.bind('msOccupancySensing', coordinatorEndpoint);
            await endpoint.configureReporting('msOccupancySensing', [{
                attribute: 'occupancy', minimumReportInterval: 1,
                maximumReportInterval: 10, reportableChange: 1,
            }]);
        }
        for (const id of [3, 4]) {
            const endpoint = device.getEndpoint(id);
            await endpoint.bind('genAnalogInput', coordinatorEndpoint);
            await endpoint.configureReporting('genAnalogInput', [{
                attribute: 'presentValue', minimumReportInterval: 1,
                maximumReportInterval: 10, reportableChange: 1,
            }, {
                attribute: 'outOfService', minimumReportInterval: 1,
                maximumReportInterval: 10,
            }]);
        }
        for (const id of [5, 6]) {
            const endpoint = device.getEndpoint(id);
            await endpoint.bind('genOnOff', coordinatorEndpoint);
            await endpoint.configureReporting('genOnOff', [{
                attribute: 'onOff', minimumReportInterval: 1,
                maximumReportInterval: 10,
            }]);
        }
        for (const id of [7, 8]) {
            const endpoint = device.getEndpoint(id);
            await endpoint.bind('genAnalogOutput', coordinatorEndpoint);
            await endpoint.configureReporting('genAnalogOutput', [{
                attribute: 'presentValue', minimumReportInterval: 1,
                maximumReportInterval: 10, reportableChange: 1,
            }]);
        }
    },
};
