#ifndef _SX127X_H_
#define _SX127X_H_

#include <stdint.h>
#include "driver/spi_master.h"

#define TTN_SPI_HOST      SPI2_HOST
#define TTN_PIN_SPI_SCLK  5
#define TTN_PIN_SPI_MOSI  27
#define TTN_PIN_SPI_MISO  19
#define TTN_PIN_NSS       18
#define TTN_PIN_RST       14
#define TTN_PIN_DIO0      26


/*
 * Register definitions
 */
#define REG_FIFO                       0x00
#define REG_OP_MODE                    0x01
#define REG_FRF_MSB                    0x06
#define REG_FRF_MID                    0x07
#define REG_FRF_LSB                    0x08
#define REG_PA_CONFIG                  0x09
#define REG_LNA                        0x0c
#define REG_FIFO_ADDR_PTR              0x0d
#define REG_FIFO_TX_BASE_ADDR          0x0e
#define REG_FIFO_RX_BASE_ADDR          0x0f
#define REG_FIFO_RX_CURRENT_ADDR       0x10
#define REG_IRQ_FLAGS                  0x12
#define REG_RX_NB_BYTES                0x13
#define REG_PKT_SNR_VALUE              0x19
#define REG_PKT_RSSI_VALUE             0x1a
#define REG_MODEM_CONFIG_1             0x1d
#define REG_MODEM_CONFIG_2             0x1e
#define REG_PREAMBLE_MSB               0x20
#define REG_PREAMBLE_LSB               0x21
#define REG_PAYLOAD_LENGTH             0x22
#define REG_MODEM_CONFIG_3             0x26
#define REG_RSSI_WIDEBAND              0x2c
#define REG_DETECTION_OPTIMIZE         0x31
#define REG_DETECTION_THRESHOLD        0x37
#define REG_SYNC_WORD                  0x39
#define REG_DIO_MAPPING_1              0x40
#define REG_VERSION                    0x42

/*
 * Transceiver modes
 */
#define MODE_LONG_RANGE_MODE           0x80
#define MODE_SLEEP                     0x00
#define MODE_STDBY                     0x01
#define MODE_TX                        0x03
#define MODE_RX_CONTINUOUS             0x05
#define MODE_RX_SINGLE                 0x06

/*
 * PA configuration
 */
#define PA_BOOST                       0x80

/*
 * IRQ masks
 */
#define IRQ_TX_DONE_MASK               0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK     0x20
#define IRQ_RX_DONE_MASK               0x40

#define PA_OUTPUT_RFO_PIN              0
#define PA_OUTPUT_PA_BOOST_PIN         1

#define TIMEOUT_RESET                  100
#define LORA_WRITE_REG_VALUE_1 0
#define LORA_WRITE_REG_VALUE_2 0x03
#define LORA_WRITE_REG_VALUE_3 0x04
#define GPIO_SET_LEVEL_LOW 0
#define GPIO_SET_LEVEL_HIGH 1
#define LORA_TX_POWER 17


#ifdef __cplusplus
extern "C" {
#endif

#define LoRa_EUROPE_FREQUENCY (838e6)

void sx127x_set_task_params(void *task);
void sx127x_configure_pins(spi_host_device_t host, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nss, uint8_t rst, uint8_t dio0);
void sx127x_init(void);
void sx127x_write_reg(uint8_t adrr, uint8_t data);
void sx127x_write_buf(uint8_t addr, uint8_t *buf, uint8_t len);
uint8_t sx127x_read_reg(uint8_t addr);
void sx127x_read_buf(uint8_t addr, uint8_t *buf, uint8_t len);
void sx127x_reset(void);
void sx127x_set_frequency(long frequency);
void sx127x_enable_crc(void);
void sx127x_send_packet(uint8_t *buf, int size);
void sx127x_receive(void);
uint8_t sx127x_received(void);
int sx127x_receive_packet(uint8_t *buf, int size);
int sx127x_packet_rssi(void);


#ifdef __cplusplus
}
#endif

#endif
