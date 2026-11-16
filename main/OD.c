#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "OD.h"
#include "sdkconfig.h"

#ifndef OD_ATTR_OD
#define OD_ATTR_OD
#endif

#define RECORD_ENTRY(pointer, index, access, size) \
    { .dataOrig = (pointer), .subIndex = (index), .attribute = (access), .dataLength = (size) }

OD_PERSIST_COMM_t OD_PERSIST_COMM = {
    .device_type = 0x000F0191,
    .manufacturer_status = 0,
    .sync_cob_id = 0x80,
    .guard_time = 0,
    .life_time_factor = 0,
    .emcy_cob_id = 0x80 + CONFIG_CANOPEN_NODE_ID,
    .producer_heartbeat_time = CONFIG_CANOPEN_HEARTBEAT_MS,
    .identity = {
        .count = 4,
        .vendor_id = 0x58,
        .product_code = 0x01401701,
        .revision_number = 0,
        .serial_number = 0,
    },
    .sdo_server = {
        .count = 2,
        .client_to_server_cob_id = 0x600 + CONFIG_CANOPEN_NODE_ID,
        .server_to_client_cob_id = 0x580 + CONFIG_CANOPEN_NODE_ID,
    },
    .rpdo_communication = {
        { .count = 2, .cob_id = 0x200 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
        { .count = 2, .cob_id = 0x300 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
        { .count = 2, .cob_id = 0x400 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
        { .count = 2, .cob_id = 0x80000500 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
    },
    .rpdo_mapping = {
        { .count = 2, .entries = { 0x62000108, 0x62000208 } },
        { .count = 4, .entries = { 0x64110110, 0x64110210, 0x64110310, 0x64110410 } },
        { .count = 4, .entries = { 0x64110510, 0x64110610, 0x64110710, 0x64110810 } },
        { .count = 0 },
    },
    .tpdo_communication = {
        { .count = 5, .cob_id = 0x180 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
        { .count = 5, .cob_id = 0x280 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
        { .count = 5, .cob_id = 0x380 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
        { .count = 5, .cob_id = 0x80000480 + CONFIG_CANOPEN_NODE_ID, .transmission_type = 255 },
    },
    .tpdo_mapping = {
        { .count = 2, .entries = { 0x60000108, 0x60000208 } },
        { .count = 4, .entries = { 0x64010110, 0x64010210, 0x64010310, 0x64010410 } },
        { .count = 4, .entries = { 0x64010510, 0x64010610, 0x64010710, 0x64010810 } },
        { .count = 0 },
    },
};

OD_RAM_t OD_RAM = {
    .error_register = 0,
    .device_name = "CO4017A",
    .mock_em_fault = 0,
    .digital_input_count = 2,
    .digital_output_count = 2,
    .analog_input_count = 8,
    .analog_output_count = 8,
};

typedef struct {
    OD_obj_var_t device_type;
    OD_obj_var_t error_register;
    OD_obj_var_t manufacturer_status;
    OD_obj_var_t sync_cob_id;
    OD_obj_var_t device_name;
    OD_obj_var_t guard_time;
    OD_obj_var_t life_time_factor;
    OD_obj_var_t emcy_cob_id;
    OD_obj_var_t producer_heartbeat_time;
    OD_obj_array_t pre_defined_error_field;
    OD_obj_record_t identity[5];
    OD_obj_record_t sdo_server[3];
    OD_obj_record_t rpdo_communication[4][3];
    OD_obj_record_t rpdo_mapping[4][5];
    OD_obj_record_t tpdo_communication[4][6];
    OD_obj_record_t tpdo_mapping[4][5];
    OD_obj_var_t mock_em_fault;
    OD_obj_record_t digital_inputs[3];
    OD_obj_record_t digital_outputs[3];
    OD_obj_record_t analog_inputs[9];
    OD_obj_record_t analog_outputs[9];
} OD_objects_t;

static CO_PROGMEM OD_objects_t OD_objects = {
    .device_type = {
        .dataOrig = &OD_PERSIST_COMM.device_type,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = sizeof(OD_PERSIST_COMM.device_type),
    },
    .error_register = {
        .dataOrig = &OD_RAM.error_register,
        .attribute = ODA_SDO_R,
        .dataLength = sizeof(OD_RAM.error_register),
    },
    .manufacturer_status = {
        .dataOrig = &OD_PERSIST_COMM.manufacturer_status,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = sizeof(OD_PERSIST_COMM.manufacturer_status),
    },
    .sync_cob_id = {
        .dataOrig = &OD_PERSIST_COMM.sync_cob_id,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = sizeof(OD_PERSIST_COMM.sync_cob_id),
    },
    .device_name = {
        .dataOrig = &OD_RAM.device_name[0],
        .attribute = ODA_SDO_R | ODA_STR,
        .dataLength = sizeof("CO4017A") - 1,
    },
    .guard_time = {
        .dataOrig = &OD_PERSIST_COMM.guard_time,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = sizeof(OD_PERSIST_COMM.guard_time),
    },
    .life_time_factor = {
        .dataOrig = &OD_PERSIST_COMM.life_time_factor,
        .attribute = ODA_SDO_RW,
        .dataLength = sizeof(OD_PERSIST_COMM.life_time_factor),
    },
    .emcy_cob_id = {
        .dataOrig = &OD_PERSIST_COMM.emcy_cob_id,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = sizeof(OD_PERSIST_COMM.emcy_cob_id),
    },
    .producer_heartbeat_time = {
        .dataOrig = &OD_PERSIST_COMM.producer_heartbeat_time,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = sizeof(OD_PERSIST_COMM.producer_heartbeat_time),
    },
    .pre_defined_error_field = {
        .dataOrig0 = NULL,
        .dataOrig = NULL,
        .attribute0 = ODA_SDO_RW,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataElementLength = sizeof(uint32_t),
        .dataElementSizeof = sizeof(uint32_t),
    },
    .mock_em_fault = {
        .dataOrig = &OD_RAM.mock_em_fault,
        .attribute = ODA_SDO_RW,
        .dataLength = sizeof(OD_RAM.mock_em_fault),
    },
    .identity = {
        RECORD_ENTRY(&OD_PERSIST_COMM.identity.count, 0, ODA_SDO_R, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_PERSIST_COMM.identity.vendor_id, 1, ODA_SDO_R | ODA_MB, sizeof(uint32_t)),
        RECORD_ENTRY(&OD_PERSIST_COMM.identity.product_code, 2, ODA_SDO_R | ODA_MB, sizeof(uint32_t)),
        RECORD_ENTRY(&OD_PERSIST_COMM.identity.revision_number, 3, ODA_SDO_R | ODA_MB, sizeof(uint32_t)),
        RECORD_ENTRY(&OD_PERSIST_COMM.identity.serial_number, 4, ODA_SDO_R | ODA_MB, sizeof(uint32_t)),
    },
    .sdo_server = {
        RECORD_ENTRY(&OD_PERSIST_COMM.sdo_server.count, 0, ODA_SDO_R, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_PERSIST_COMM.sdo_server.client_to_server_cob_id, 1, ODA_SDO_R | ODA_MB, sizeof(uint32_t)),
        RECORD_ENTRY(&OD_PERSIST_COMM.sdo_server.server_to_client_cob_id, 2, ODA_SDO_R | ODA_MB, sizeof(uint32_t)),
    },
    .rpdo_communication = {
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[0].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[0].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[0].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[1].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[1].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[1].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[2].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[2].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[2].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[3].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[3].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_communication[3].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
        },
    },
    .rpdo_mapping = {
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[0].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[0].entries[0], 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[0].entries[1], 2, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[1].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[1].entries[0], 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[1].entries[1], 2, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[1].entries[2], 3, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[1].entries[3], 4, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[2].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[2].entries[0], 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[2].entries[1], 2, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[2].entries[2], 3, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[2].entries[3], 4, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.rpdo_mapping[3].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
        },
    },
    .tpdo_communication = {
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[0].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[0].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[0].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[0].inhibit_time, 3, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[0].compatibility_entry, 4, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[0].event_timer, 5, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[1].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[1].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[1].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[1].inhibit_time, 3, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[1].compatibility_entry, 4, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[1].event_timer, 5, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[2].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[2].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[2].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[2].inhibit_time, 3, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[2].compatibility_entry, 4, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[2].event_timer, 5, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[3].count, 0, ODA_SDO_R, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[3].cob_id, 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[3].transmission_type, 2, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[3].inhibit_time, 3, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[3].compatibility_entry, 4, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_communication[3].event_timer, 5, ODA_SDO_RW | ODA_MB, sizeof(uint16_t)),
        },
    },
    .tpdo_mapping = {
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[0].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[0].entries[0], 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[0].entries[1], 2, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[1].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[1].entries[0], 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[1].entries[1], 2, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[1].entries[2], 3, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[1].entries[3], 4, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[2].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[2].entries[0], 1, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[2].entries[1], 2, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[2].entries[2], 3, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[2].entries[3], 4, ODA_SDO_RW | ODA_MB, sizeof(uint32_t)),
        },
        {
            RECORD_ENTRY(&OD_PERSIST_COMM.tpdo_mapping[3].count, 0, ODA_SDO_RW, sizeof(uint8_t)),
        },
    },
    .digital_inputs = {
        RECORD_ENTRY(&OD_RAM.digital_input_count, 0, ODA_SDO_R, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_RAM.digital_inputs[0], 1, ODA_SDO_R | ODA_TPDO, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_RAM.digital_inputs[1], 2, ODA_SDO_R | ODA_TPDO, sizeof(uint8_t)),
    },
    .digital_outputs = {
        RECORD_ENTRY(&OD_RAM.digital_output_count, 0, ODA_SDO_R, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_RAM.digital_outputs[0], 1, ODA_SDO_RW | ODA_RPDO, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_RAM.digital_outputs[1], 2, ODA_SDO_RW | ODA_RPDO, sizeof(uint8_t)),
    },
    .analog_inputs = {
        RECORD_ENTRY(&OD_RAM.analog_input_count, 0, ODA_SDO_R, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[0], 1, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[1], 2, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[2], 3, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[3], 4, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[4], 5, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[5], 6, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[6], 7, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_inputs[7], 8, ODA_SDO_R | ODA_TPDO | ODA_MB, sizeof(int16_t)),
    },
    .analog_outputs = {
        RECORD_ENTRY(&OD_RAM.analog_output_count, 0, ODA_SDO_R, sizeof(uint8_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[0], 1, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[1], 2, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[2], 3, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[3], 4, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[4], 5, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[5], 6, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[6], 7, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
        RECORD_ENTRY(&OD_RAM.analog_outputs[7], 8, ODA_SDO_RW | ODA_RPDO | ODA_MB, sizeof(int16_t)),
    },
};

static OD_ATTR_OD OD_entry_t OD_list[] = {
    { 0x1000, 1, ODT_VAR, &OD_objects.device_type, NULL },
    { 0x1001, 1, ODT_VAR, &OD_objects.error_register, NULL },
    { 0x1002, 1, ODT_VAR, &OD_objects.manufacturer_status, NULL },
    { 0x1003, OD_CNT_ARR_1003 + 1, ODT_ARR, &OD_objects.pre_defined_error_field, NULL },
    { 0x1005, 1, ODT_VAR, &OD_objects.sync_cob_id, NULL },
    { 0x1008, 1, ODT_VAR, &OD_objects.device_name, NULL },
    { 0x100C, 1, ODT_VAR, &OD_objects.guard_time, NULL },
    { 0x100D, 1, ODT_VAR, &OD_objects.life_time_factor, NULL },
    { 0x1014, 1, ODT_VAR, &OD_objects.emcy_cob_id, NULL },
    { 0x1017, 1, ODT_VAR, &OD_objects.producer_heartbeat_time, NULL },
    { 0x1018, 5, ODT_REC, &OD_objects.identity, NULL },
    { 0x1200, 3, ODT_REC, &OD_objects.sdo_server, NULL },
    { 0x1400, 3, ODT_REC, &OD_objects.rpdo_communication[0], NULL },
    { 0x1401, 3, ODT_REC, &OD_objects.rpdo_communication[1], NULL },
    { 0x1402, 3, ODT_REC, &OD_objects.rpdo_communication[2], NULL },
    { 0x1403, 3, ODT_REC, &OD_objects.rpdo_communication[3], NULL },
    { 0x1600, 3, ODT_REC, &OD_objects.rpdo_mapping[0], NULL },
    { 0x1601, 5, ODT_REC, &OD_objects.rpdo_mapping[1], NULL },
    { 0x1602, 5, ODT_REC, &OD_objects.rpdo_mapping[2], NULL },
    { 0x1603, 1, ODT_REC, &OD_objects.rpdo_mapping[3], NULL },
    { 0x1800, 6, ODT_REC, &OD_objects.tpdo_communication[0], NULL },
    { 0x1801, 6, ODT_REC, &OD_objects.tpdo_communication[1], NULL },
    { 0x1802, 6, ODT_REC, &OD_objects.tpdo_communication[2], NULL },
    { 0x1803, 6, ODT_REC, &OD_objects.tpdo_communication[3], NULL },
    { 0x1A00, 3, ODT_REC, &OD_objects.tpdo_mapping[0], NULL },
    { 0x1A01, 5, ODT_REC, &OD_objects.tpdo_mapping[1], NULL },
    { 0x1A02, 5, ODT_REC, &OD_objects.tpdo_mapping[2], NULL },
    { 0x1A03, 1, ODT_REC, &OD_objects.tpdo_mapping[3], NULL },
    { 0x2F00, 1, ODT_VAR, &OD_objects.mock_em_fault, NULL },
    { 0x6000, 3, ODT_REC, &OD_objects.digital_inputs, NULL },
    { 0x6200, 3, ODT_REC, &OD_objects.digital_outputs, NULL },
    { 0x6401, 9, ODT_REC, &OD_objects.analog_inputs, NULL },
    { 0x6411, 9, ODT_REC, &OD_objects.analog_outputs, NULL },
    { 0x0000, 0, 0, NULL, NULL },
};

static OD_t od = {
    (sizeof(OD_list) / sizeof(OD_list[0])) - 1,
    &OD_list[0],
};

OD_t *OD = &od;
