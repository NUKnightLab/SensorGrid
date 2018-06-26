#ifndef SENSORGRIDPM_LORA_H_
#define SENSORGRIDPM_LORA_H_

#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#include "config.h"

#define NODE_ID 1
#define REQUIRED_RH_VERSION_MAJOR 1
#define REQUIRED_RH_VERSION_MINOR 82
#define FREQ 915.00
#define TX 5
#define CAD_TIMEOUT 1000
#define ROUTER_TIMEOUT 2000
#define USE_MESH_ROUTER 0
#define USE_SLOW_RELIABLE_MODE 0

/* LoRa */
extern RH_RF95 *radio;
/* Mesh routing is no longer officially supported. This option is kept here for
   experimental and development purposes, but support for mesh routing is
   fully deprecated. We will instead be working to make static routing as
   straightforward as possible. The arping and route establishment process of
   RadioHead's mesh router is too heavy, relying too much on repeated
   beaconization and messaging retries. It also aggressively drops routes on
   failure, causing persistent thrashing of route definitions. Also, there is a
   logic error in the route establishment protocol that causes it to always drop
   packets when first establishing the route and only successfully delivering
   them in subsequent transmissions, provided the route has not been otherwise
   dropped in the mean time. We may explore mesh routing again after we have a
   stable production ready system. */
#if defined(USE_MESH_ROUTER)
extern RHMesh* router;
#else
extern RHRouter* router;
#endif

extern void setupRadio(uint8_t cs_pint, uint8_t int_pin, uint8_t node_id);
enum {
    RECV_STATUS_SUCCESS = 0,
    RECV_STATUS_WRONG_VERSION = -1,
    RECV_STATUS_WRONG_NETWORK = -2,
    RECV_STATUS_NO_MESSAGE = -3
};

extern void setup_radio(uint8_t cs_pin, uint8_t int_pin, uint8_t node_id);
extern int receive(Message *msg, uint16_t timeout);
extern uint8_t send_message(uint8_t *msg, uint8_t len, uint8_t to_id);

#endif  // SENSORGRIDPM_LORA_H_
