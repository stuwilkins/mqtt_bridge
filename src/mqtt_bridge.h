/*
 * =====================================================================================
 *
 *       Filename:  mqtt_bridge.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  21/07/18 11:35:59
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Stuart B. Wilkins (sbw), stuwilkins@mac.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef MQTT_BRIDGE_H
#define MQTT_BRIDGE_H

#define MQTT_BRIDGE_MAX_TOPICS      1024
#define MQTT_BRIDGE_MAX_STRLEN      1024

#define MQTT_BRIDGE_TB_TOPIC        "v1/devices/me/telemetry"

typedef struct {
  char topic[MQTT_BRIDGE_MAX_STRLEN];
  char key[MQTT_BRIDGE_MAX_STRLEN];
  int qos;
  double factor;
} homeauto_topic;

typedef struct {
  MQTTAsync client;
  int connected;
  char server[MQTT_BRIDGE_MAX_STRLEN];
  char username[MQTT_BRIDGE_MAX_STRLEN];
  char password[MQTT_BRIDGE_MAX_STRLEN];
  char client_id[MQTT_BRIDGE_MAX_STRLEN];
} mqtt_connection;

typedef struct {
  homeauto_topic topic[MQTT_BRIDGE_MAX_TOPICS];
  int topics;
  int verbose;
  mqtt_connection mqtt_in;
  mqtt_connection mqtt_out;
} homeauto_data;

#endif
