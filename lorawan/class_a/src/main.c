/*********************************************************************
 * COPYRIGHT 2022 CONNECTED DEVELOPMENT, A DIVISION OF EXPONENTIAL
 * TECHNOLOGY GROUP.
 *
 * This implements a Class A LoRaWAN application.
 *
 * Copied from the example in the nRF Connect SDK v2.1.0 at:
 *     zephyr\samples\subsys\lorawan
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

/*
 * Class A LoRaWAN sample application
 *
 * Copyright (c) 2020 Manivannan Sadhasivam <mani@kernel.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <lorawan/lorawan.h>
#include <zephyr.h>

/* Customize based on network configuration */
#define LORAWAN_DEV_EUI       { 0x00, 0x80, 0x00, 0x00, 0x04, 0x01, 0xdd, 0x40 }
#define LORAWAN_JOIN_EUI      { 0xa2, 0xb3, 0x84, 0x25, 0xcf, 0xb6, 0xf7, 0xfe }
#define LORAWAN_APP_KEY       { 0x87, 0x47, 0xcc, 0xc8, 0xce, 0x01, 0xd2, 0x96, 0x2d, 0x5f, 0x94, 0x60, 0x0b, 0xcd, 0x38, 0xcf }

#define DELAY K_MSEC(4000)

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(lorawan_class_a);

char data[] = {'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd', '0', '0', '0', '0'};

static void dl_callback(uint8_t port, bool data_pending,
                        int16_t rssi, int8_t snr,
                        uint8_t len, const uint8_t *data)
{
   LOG_INF("RX Port %d, Pending %d, RSSI %ddB, SNR %ddBm", port, data_pending, rssi, snr);
   if (data)
   {
      LOG_HEXDUMP_INF(data, len, "RX Payload: ");
   }
}

static void lorwan_datarate_changed(enum lorawan_datarate dr)
{
   uint8_t unused, max_size;

   lorawan_get_payload_sizes(&unused, &max_size);
   LOG_INF("New Datarate: DR_%d, Max Payload %d", dr, max_size);
}

void main(void)
{
   const struct device *lora_dev;
   struct lorawan_join_config join_cfg;
   uint8_t dev_eui[] = LORAWAN_DEV_EUI;
   uint8_t join_eui[] = LORAWAN_JOIN_EUI;
   uint8_t app_key[] = LORAWAN_APP_KEY;
   int ret;
   uint32_t counter = 0;

   struct lorawan_downlink_cb downlink_cb =
   {
      .port = LW_RECV_PORT_ANY,
      .cb = dl_callback
   };

   LOG_INF("Version 1.0  Build: %s %s", __DATE__, __TIME__);

   lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
   if (!device_is_ready(lora_dev))
   {
      LOG_ERR("%s: device not ready.", lora_dev->name);
      return;
   }

   ret = lorawan_start();
   if (ret < 0)
   {
      LOG_ERR("lorawan_start failed: %d", ret);
      return;
   }

   lorawan_register_downlink_callback(&downlink_cb);
   lorawan_register_dr_changed_callback(lorwan_datarate_changed);

   join_cfg.mode = LORAWAN_ACT_OTAA;
   join_cfg.dev_eui = dev_eui;
   join_cfg.otaa.join_eui = join_eui;
   join_cfg.otaa.app_key = app_key;
   join_cfg.otaa.nwk_key = app_key;

   for (int i = 0; i < 16; i++)
   {
      LOG_INF("Joining network over OTAA. Attempt #%d", i);
      ret = lorawan_join(&join_cfg);
      if (ret < 0)
      {
         LOG_ERR("lorawan_join_network failed: %d", ret);

         if ((i % 2) == 1)
         {
            k_sleep(K_MSEC(6000));
         }
      }
      else
      {
         i = 0xFF;
      }
   }

   while (1)
   {
      // Append the counter.
      counter++;
      data[10] = (counter >> 24) & 0xFF; // MSB
      data[11] = (counter >> 16) & 0xFF;
      data[12] = (counter >>  8) & 0xFF;
      data[13] = (counter >>  0) & 0xFF; // LSB

      LOG_INF("Sending confirmed data. Count=%u", counter);

      ret = lorawan_send(2, data, sizeof(data), LORAWAN_MSG_CONFIRMED);

      /*
       * Note: The stack may return -EAGAIN if the provided data
       * length exceeds the maximum possible one for the region and
       * datarate. But since we are just sending the same data here,
       * we'll just continue.
       */
      if (ret == -EAGAIN)
      {
         LOG_ERR("lorawan_send failed: %d. Continuing...", ret);
         k_sleep(DELAY);
         continue;
      }

      if (ret < 0)
      {
         LOG_ERR("lorawan_send failed: %d", ret);
         return;
      }

      LOG_INF("Data sent!");
      k_sleep(DELAY);
   }
}
