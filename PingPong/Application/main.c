/*********************************************************************
 * COPYRIGHT 2022 CONNECTED DEVELOPMENT, A DIVISION OF EXPONENTIAL
 * TECHNOLOGY GROUP.
 *
 * This implements a simple LoRa transmit and receive application.
 * Two devices are used, each side running this application. One side
 * sends PING messages, and the other side responds with PONG
 * messages.
 *
 * Copied from examples from the nRF Connect SDK v2.0.2 at:
 *     zephyr\samples\drivers\lora\receive
 *     zephyr\samples\drivers\lora\send
 *     modules\lib\loramac-node\src\apps\ping-pong\NucleoL073\main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

/*
 * Copyright (c) 2019 Manivannan Sadhasivam
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/gpio.h>
#include <drivers/lora.h>
#include <errno.h>
#include <sys/util.h>
#include <radio.h>
#include <zephyr.h>

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)
BUILD_ASSERT(DT_NODE_HAS_STATUS(DEFAULT_RADIO_NODE, okay),
             "No default LoRa radio specified in DT");

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(LoraPingPong);

typedef enum
{
   LOWPOWER,
   RX,
   RX_TIMEOUT,
   RX_ERROR,
   TX,
   TX_TIMEOUT,
   TX_ERROR
} States_t;

#define RX_TIMEOUT_VALUE                     2000     // msec
#define TX_TIME_VALUE                        500      // msec
#define BUFFER_SIZE                          64       // Define the payload size here

static const uint8_t pingMsg[] = "PING";
static const uint8_t pongMsg[] = "PONG";

static uint16_t bufferSize = BUFFER_SIZE;
static uint8_t  buffer[BUFFER_SIZE];

static States_t state = LOWPOWER;

static int16_t  rssiValue = 0;
static int8_t   snrValue = 0;
static uint32_t pingCounter = 0;   // Send a different number with each PING packet.
static uint32_t rxCounter = 0;   // Counter received in a PING or PONG packet.

#ifdef USE_LEDS
   #define LED1_NODE    DT_ALIAS(led0)
   #define LED2_NODE    DT_ALIAS(led1)
   static const struct gpio_dt_spec led1Rx = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
   static const struct gpio_dt_spec led2Tx = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
#endif

static const struct device *lora_dev = DEVICE_DT_GET(DEFAULT_RADIO_NODE);


/**
 * @brief Receive data.
 *
 * @param timeoutMs  Time to wait in msec.
 */
static void RxLora(uint32_t timeoutMs)
{
   // Block until data arrives or timeout.
   //   Calls sx12xx_lora_recv().
   int size = lora_recv(lora_dev, buffer, sizeof(buffer), K_MSEC(timeoutMs), &rssiValue, &snrValue);

   // Timeout?
   if (size == -EAGAIN)
   {
      // Log is printed in sx12xx_lora_recv().
      Radio.Sleep();
      state = RX_TIMEOUT;
   }
   else if (size < 0)
   {
      LOG_ERR("Receive failed");
      Radio.Sleep();
      state = RX_ERROR;
   }
   else  // Success
   {
      LOG_HEXDUMP_DBG(buffer, size, "Received:");

      Radio.Sleep();
      bufferSize = size;
      state = RX;
   }
}

/**
 * @brief Transmit data and wait for Tx Done.
 *
 * @param buffer  Buffer pointer
 * @param size    Buffer size
 */
static void TxLora(uint8_t *buffer, uint8_t size)
{
   // Calls sx12xx_lora_send().
   // Note that Tx timeout prints an ERROR log, but is not indicated in ret.
   int ret = lora_send(lora_dev, buffer, size);
   if (ret < 0)
   {
      LOG_ERR("Send failed");

      Radio.Sleep();
      state = TX_ERROR;
   }
   else  // Success
   {
      LOG_HEXDUMP_DBG(buffer, size, "Tx Done:");
      Radio.Sleep();
      state = TX;
   }
}

/**
 * @brief Transmit a PING packet with an incrementing packet counter.
 */
static void SendPing(void)
{
   // Send the next PING frame.
   buffer[0] = 'P';
   buffer[1] = 'I';
   buffer[2] = 'N';
   buffer[3] = 'G';

   // Append the pingCounter.
   pingCounter++;
   buffer[4] = (pingCounter >> 24) & 0xFF; // MSB
   buffer[5] = (pingCounter >> 16) & 0xFF;
   buffer[6] = (pingCounter >>  8) & 0xFF;
   buffer[7] = (pingCounter >>  0) & 0xFF; // LSB

   // Fill the remaining payload with numbers.
   for (int i = 8; i < bufferSize; i++)
   {
      buffer[i] = i - 8;
   }

   k_sleep(K_MSEC(1));
   TxLora(buffer, bufferSize);

   LOG_INF("Sent PING. Counter=%u", pingCounter);

}

static void PingPong(void)
{
   bool isMaster = true;

   RxLora(RX_TIMEOUT_VALUE);

   while (1)
   {
      switch (state)
      {
         case RX:
            state = LOWPOWER;
            if (isMaster == true)
            {
               if (bufferSize > 0)
               {
                  if (strncmp((const char *) buffer, (const char *) pongMsg, 4) == 0)
                  {
#ifdef USE_LEDS
                     // Toggle the Rx LED.
                     gpio_pin_toggle_dt(&led1Rx);
#endif

                     rxCounter = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | (buffer[7]);
                     LOG_INF("Received PONG: %u (RSSI:%ddBm, SNR:%ddBm)",
                             rxCounter, rssiValue, snrValue);

                     // Delay between PINGs to set the pace.
                     k_sleep(K_MSEC(TX_TIME_VALUE));

                     // Send the next PING frame.
                     SendPing();
                  }
                  else if (strncmp((const char *) buffer, (const char *) pingMsg, 4) == 0)
                  { // A master already exists then become a slave.
                     isMaster = false;
                     LOG_HEXDUMP_INF(buffer, 8, "Received PING:");

#ifdef USE_LEDS
                     // Turn LEDs off.
                     gpio_pin_set_dt(&led2Tx, 0);
                     gpio_pin_set_dt(&led1Rx, 1);
#endif

                     RxLora(RX_TIMEOUT_VALUE);
                  }
                  else // Valid reception but neither a PING or a PONG message.
                  {    // Set device as master and start again.
                     isMaster = true;
                     LOG_HEXDUMP_INF(buffer, 8, "Master received data:");
                     RxLora(RX_TIMEOUT_VALUE);
                  }
               }
            }
            else
            {
               if (bufferSize > 0)
               {
                  if (strncmp((const char *) buffer, (const char *) pingMsg, 4) == 0)
                  {
#ifdef USE_LEDS
                     // Toggle the Rx LED.
                     gpio_pin_toggle_dt(&led1Rx);
#endif

                     rxCounter = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | (buffer[7]);
                     LOG_INF("Received PING: %u (RSSI:%ddBm, SNR:%ddBm)",
                             rxCounter, rssiValue, snrValue);

                     // Send the PONG reply to the PING string.
                     // Overwrite the PING in the buffer we just received, but keep
                     // the counter and remaining payload as-is.
                     buffer[0] = 'P';
                     buffer[1] = 'O';
                     buffer[2] = 'N';
                     buffer[3] = 'G';

                     k_sleep(K_MSEC(1));

                     TxLora(buffer, bufferSize);

                     rxCounter = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | (buffer[7]);
                     LOG_INF("Sent PONG. Counter=%u", rxCounter);
                  }
                  else // valid reception but not a PING as expected.
                  {    // Set device as master and start again.
                     isMaster = true;
                     LOG_HEXDUMP_INF(buffer, 8, "Received data:");
                     RxLora(RX_TIMEOUT_VALUE);
                  }
               }
            }
            break;

         case TX:
#ifdef USE_LEDS
            // Toggle the Tx LED.
            gpio_pin_toggle_dt(&led2Tx);
#endif
            RxLora(RX_TIMEOUT_VALUE);
            break;

         case RX_TIMEOUT:
         case RX_ERROR:
            if (isMaster == true)
            {
               // Send the next PING frame.
               SendPing();
            }
            else
            {
               RxLora(RX_TIMEOUT_VALUE);
            }
            break;

         case TX_ERROR:
         case TX_TIMEOUT:
            RxLora(RX_TIMEOUT_VALUE);
            break;

         case LOWPOWER:
         default:
            // Set low power
            break;
      }

//      BoardLowPowerHandler();
   }
}

void main(void)
{
   struct lora_modem_config config;
   int ret;

   // The SX126x chip is initialized in RadioInit() prior to entering here.

   // Delay to allow debug logs from prior init functions to print.
   k_sleep(K_MSEC(500));

   LOG_INF("Version 2.1  Build: %s %s", __DATE__, __TIME__);

   if (!device_is_ready(lora_dev))
   {
      LOG_ERR("%s Device not ready", lora_dev->name);
      return;
   }

#ifdef USE_LEDS
   if (!device_is_ready(led1Rx.port))
   {
      LOG_ERR("%s: LED1 not ready", DT_NODE_FULL_NAME(LED1_NODE));
      return;
   }

   ret = gpio_pin_configure_dt(&led1Rx, GPIO_OUTPUT_INACTIVE);
   if (ret < 0)
   {
      LOG_ERR("%s: LED1 config error",  DT_NODE_FULL_NAME(LED1_NODE));
      return;
   }

   if (!device_is_ready(led2Tx.port))
   {
      LOG_ERR("%s: LED2 not ready",  DT_NODE_FULL_NAME(LED2_NODE));
      return;
   }

   ret = gpio_pin_configure_dt(&led2Tx, GPIO_OUTPUT_INACTIVE);
   if (ret < 0)
   {
      LOG_ERR("%s: LED2 config error", DT_NODE_FULL_NAME(LED2_NODE));
      return;
   }
#endif

   config.frequency = 915000000;   // Frequency in Hz
   config.bandwidth = BW_500_KHZ;  // Selects LORA_BW_500 = 6
   config.datarate = SF_10;        // SpreadingFactor=10.
   config.preamble_len = 12;
   config.coding_rate = CR_4_5;
   config.tx_power = 22;  // Max. Set in SX126xSetRfTxPower() - Also sets RampTime = 2 (RADIO_RAMP_40_US)
                          // Also calls SX126xSetPaConfig(paDutyCycle=0x04, hpMax=0x07, deviceSel=0x00, paLut=0x01)

   // Must set lora packet type before setting the frequency in lora_config().
   Radio.SetModem(MODEM_LORA);

   LOG_INF("Call lora_config() for TX.");
   config.tx = true;

   //   Calls:
   //     sx12xx_lora_config()
   //       RadioSetChannel(config->frequency);
   //       RadioSetTxConfig(MODEM_LORA, config->tx_power, 0,
   //                        config->bandwidth, config->datarate,
   //                        config->coding_rate, config->preamble_len,
   //                        false, true, 0, 0, false, 4000);
   ret = lora_config(lora_dev, &config);
   if (ret < 0)
   {
      LOG_ERR("Tx config failed");
      return;
   }

   LOG_INF("Call lora_config() for RX.");
   config.tx = false;

   //   Calls:
   //     sx12xx_lora_config()
   //       RadioSetChannel(config->frequency);
   //       RadioSetRxConfig(MODEM_LORA, config->bandwidth,
   //                        config->datarate, config->coding_rate,
   //                        0, config->preamble_len, 10, false, 0,
   //                        false, 0, 0, false, true);
   ret = lora_config(lora_dev, &config);
   if (ret < 0)
   {
      LOG_ERR("Rx config failed");
      return;
   }

   LOG_INF("lora_config() Rx,Tx success.");
   k_sleep(K_MSEC(300));

   pingCounter = 0;

   // Run the ping-pong state machine.
   PingPong();
   // Never returns!

   k_sleep(K_FOREVER);
}

