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

#define MAX_TOPICS 1024

typedef struct {
  char topic[256];
  int qos;
  double factor;
  char key[256];
} homeauto_topic;

typedef struct {
  MQTTAsync client;
  int connected;
  char server[256];
  char username[256];
  char password[256];
  char client_id[256];
} mqtt_connection;

typedef struct {
  homeauto_topic topic[MAX_TOPICS];
  int topics;
  int verbose;
  mqtt_connection mqtt_in;
  mqtt_connection mqtt_out;
} homeauto_data;

#endif
