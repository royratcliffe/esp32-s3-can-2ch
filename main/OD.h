#ifndef OD_H
#define OD_H

#include <stdint.h>

#include "301/CO_ODinterface.h"

#define OD_CNT_NMT 1
#define OD_CNT_EM 1
#define OD_CNT_EM_PROD 1
#define OD_CNT_HB_PROD 1
#define OD_CNT_SDO_SRV 1
#define OD_CNT_ARR_1003 8
#define OD_CNT_RPDO 4
#define OD_CNT_TPDO 4

typedef struct {
    uint8_t count;
    uint32_t entries[4];
} OD_mapping_t;

typedef struct {
    uint8_t count;
    uint32_t cob_id;
    uint8_t transmission_type;
} OD_rpdo_communication_t;

typedef struct {
    uint8_t count;
    uint32_t cob_id;
    uint8_t transmission_type;
    uint16_t inhibit_time;
    uint16_t compatibility_entry;
    uint16_t event_timer;
} OD_tpdo_communication_t;

typedef struct {
    uint32_t device_type;
    uint32_t manufacturer_status;
    uint32_t sync_cob_id;
    uint16_t guard_time;
    uint8_t life_time_factor;
    uint32_t emcy_cob_id;
    uint16_t producer_heartbeat_time;
    struct {
        uint8_t count;
        uint32_t vendor_id;
        uint32_t product_code;
        uint32_t revision_number;
        uint32_t serial_number;
    } identity;
    struct {
        uint8_t count;
        uint32_t client_to_server_cob_id;
        uint32_t server_to_client_cob_id;
    } sdo_server;
    OD_rpdo_communication_t rpdo_communication[4];
    OD_mapping_t rpdo_mapping[4];
    OD_tpdo_communication_t tpdo_communication[4];
    OD_mapping_t tpdo_mapping[4];
} OD_PERSIST_COMM_t;

typedef struct {
    uint8_t error_register;
    char device_name[8];
    uint8_t mock_em_fault;
    uint8_t digital_input_count;
    uint8_t digital_inputs[2];
    uint8_t digital_output_count;
    uint8_t digital_outputs[2];
    uint8_t analog_input_count;
    int16_t analog_inputs[8];
    uint8_t analog_output_count;
    int16_t analog_outputs[8];
} OD_RAM_t;

extern OD_PERSIST_COMM_t OD_PERSIST_COMM;
extern OD_RAM_t OD_RAM;
extern OD_t *OD;

#define OD_ENTRY(index) (&OD->list[index])
#define OD_ENTRY_H1000 OD_ENTRY(0)
#define OD_ENTRY_H1001 OD_ENTRY(1)
#define OD_ENTRY_H1002 OD_ENTRY(2)
#define OD_ENTRY_H1003 OD_ENTRY(3)
#define OD_ENTRY_H1005 OD_ENTRY(4)
#define OD_ENTRY_H1008 OD_ENTRY(5)
#define OD_ENTRY_H100C OD_ENTRY(6)
#define OD_ENTRY_H100D OD_ENTRY(7)
#define OD_ENTRY_H1014 OD_ENTRY(8)
#define OD_ENTRY_H1017 OD_ENTRY(9)
#define OD_ENTRY_H1018 OD_ENTRY(10)
#define OD_ENTRY_H1200 OD_ENTRY(11)
#define OD_ENTRY_H1400 OD_ENTRY(12)
#define OD_ENTRY_H1401 OD_ENTRY(13)
#define OD_ENTRY_H1402 OD_ENTRY(14)
#define OD_ENTRY_H1403 OD_ENTRY(15)
#define OD_ENTRY_H1600 OD_ENTRY(16)
#define OD_ENTRY_H1601 OD_ENTRY(17)
#define OD_ENTRY_H1602 OD_ENTRY(18)
#define OD_ENTRY_H1603 OD_ENTRY(19)
#define OD_ENTRY_H1800 OD_ENTRY(20)
#define OD_ENTRY_H1801 OD_ENTRY(21)
#define OD_ENTRY_H1802 OD_ENTRY(22)
#define OD_ENTRY_H1803 OD_ENTRY(23)
#define OD_ENTRY_H1A00 OD_ENTRY(24)
#define OD_ENTRY_H1A01 OD_ENTRY(25)
#define OD_ENTRY_H1A02 OD_ENTRY(26)
#define OD_ENTRY_H1A03 OD_ENTRY(27)
#define OD_ENTRY_H2F00 OD_ENTRY(28)

#endif
