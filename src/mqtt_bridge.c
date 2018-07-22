/*
 * =====================================================================================
 *
 *       Filename:  mqtt_broker.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  21/07/18 11:04:37
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Stuart B. Wilkins (sbw), stuwilkins@mac.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <libconfig.h>
#include <sys/types.h>
#include <MQTTAsync.h>
#include <getopt.h> 
#include "mqtt_bridge.h"

void mqtt_on_success(void* context, MQTTAsync_successData* response)
{
  homeauto_data *data = (homeauto_data*)context;
  if(data->verbose)
  {
	  fprintf(stderr, "Message with token value %d delivery confirmed\n", response->token);
  }
}

int mqtt_publish_thingsboard(homeauto_data *data, uint32_t ts, char *key, double value)
{
  char data_buffer[1024];
  snprintf(data_buffer, 256, "{\"ts\":%u000, \"values\":{\"%s\":%lf}}", ts, key, value);

  if(data->verbose)
  {
    fprintf(stderr, "Sending %s\n", data_buffer);
  }

  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  opts.onSuccess = mqtt_on_success;
  opts.context = data;
  pubmsg.payload = data_buffer;
	pubmsg.payloadlen = strlen(data_buffer);
  pubmsg.qos = 1;
  pubmsg.retained = 0;
  
  MQTTAsync_sendMessage(data->mqtt_out.client, "v1/devices/me/telemetry", &pubmsg, &opts);
}

int mqtt_message_callback(void *context, char *topic_name, int topic_len, MQTTAsync_message *message)
{
    char *payloadptr = message->payload;
    homeauto_data *data = (homeauto_data*)context;

    if(message->payloadlen != 8)
    {
      MQTTAsync_freeMessage(&message);
      MQTTAsync_free(topic_name);
      return 0;
    }

    uint32_t _ts = 0;
    _ts |= payloadptr[0] << 24;
    _ts |= payloadptr[1] << 16;
    _ts |= payloadptr[2] << 8;
    _ts |= payloadptr[3];

    int32_t _data = 0;
    _data |= payloadptr[4] << 24;
    _data |= payloadptr[5] << 16;
    _data |= payloadptr[6] << 8;
    _data |= payloadptr[7];

    int i;
    for(i=0;i<data->topics;i++)
    {
      if(!strcmp(data->topic[i].topic, topic_name))
      {
        if(data->verbose){
          printf("Message %d: Topic = %s Timestamp = %x , Value = %f\n", i, topic_name, 
              _ts, (double)_data/data->topic[i].factor);
        }
        mqtt_publish_thingsboard(data, _ts, data->topic[i].key, (float)_data/data->topic[i].factor);
      }
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic_name);
    return 1;
}

void mqtt_in_connect(void* context, MQTTAsync_successData* response)
{
	fprintf(stderr, "MQTT Connectng to IN\n");
	homeauto_data* data = (homeauto_data*)context;

	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	opts.context = data;
	
	int rc, i;
  for(i=0;i<data->topics;i++)
  {
    if ((rc = MQTTAsync_subscribe(data->mqtt_in.client, data->topic[i].topic, 
            data->topic[i].qos, &opts)) != MQTTASYNC_SUCCESS)
    {
      fprintf(stderr, "Failed to subscribe to , return code %d\n", data->topic[i].topic, rc);
    } else {
      fprintf(stderr, "Subscribed to %s\n", data->topic[i].topic);
    }
  }

	fprintf(stderr, "MQTT Connection IN connected.\n");
	data->mqtt_in.connected = 1;
}

void mqtt_out_connect(void* context, MQTTAsync_successData* response)
{
	homeauto_data* data = (homeauto_data*)context;
	fprintf(stderr, "MQTT Connection OUT connected.\n");
	data->mqtt_out.connected = 1;	
}

void mqtt_out_connlost(void *context, char *cause)
{
	homeauto_data* data = (homeauto_data*)context;
  data->mqtt_out.connected = 0;

	MQTTAsync client = data->mqtt_out.client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
  conn_opts.MQTTVersion = MQTTVERSION_3_1;

	fprintf(stderr, "Connection lost cause: %s Reconnecting ...", cause);

	int rc;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		fprintf(stderr, "Failed to start connect, return code %d\n", rc);
	} else {
    data->mqtt_out.connected = 1;
  }
}

void mqtt_in_connlost(void *context, char *cause)
{
	homeauto_data* data = (homeauto_data*)context;
  data->mqtt_in.connected = 0;

	MQTTAsync client = data->mqtt_in.client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
  conn_opts.MQTTVersion = MQTTVERSION_3_1;

	fprintf(stderr, "Connection lost cause: %s Reconnecting ...", cause);

	int rc;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		fprintf(stderr, "Failed to start connect, return code %d\n", rc);
	} else {
		fprintf(stderr, "Connected.");
		data->mqtt_in.connected = 1;
  }
}

int read_config_server(config_t *cfg, mqtt_connection *mqtt, const char *key)
{
  const char* _server;
  const char* _client_id;
  const char* _username;
  const char* _password;

  config_setting_t *setting = config_lookup(cfg, key);

  if((config_setting_lookup_string(setting, "url", &_server) 
        && config_setting_lookup_string(setting, "client_id", &_client_id)))
  {
    strcpy(mqtt->server, _server);
    strcpy(mqtt->client_id, _client_id);
  } else {
    return 0;
  }

  if(config_setting_lookup_string(setting, "username", &_username))
  {
    strcpy(mqtt->username, _username);
  } else {
    strcpy(mqtt->username, "");
  }

  if(config_setting_lookup_string(setting, "password", &_password))
  {
    strcpy(mqtt->password, _password);
  } else {
    strcpy(mqtt->password, "");
  }
  
  fprintf(stderr, "%s : server = %s, client_id = %s, username = %s, password = %s\n", 
      key, mqtt->server, mqtt->client_id, mqtt->username, mqtt->password);

  return 1;
}

int read_config(homeauto_data *data, const char* config_file)
{
  config_t cfg;
  config_setting_t *setting;

  config_init(&cfg);

  if(!config_read_file(&cfg, config_file)) 
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
        config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return 0;
  }

  read_config_server(&cfg, &data->mqtt_in, "server_in"); 
  read_config_server(&cfg, &data->mqtt_out, "server_out"); 

  setting = config_lookup(&cfg, "telemetry_data");
  if(setting != NULL)
  {
    unsigned int count = config_setting_length(setting);
    unsigned int i;
    unsigned int j = 0;
    for(i = 0; i < count; ++i)
    {
      config_setting_t *datum = config_setting_get_elem(setting, i);
      const char* _topic;
      const char* _key;
      double _factor;
      int _qos;
      if(!(config_setting_lookup_string(datum, "key", &_key)
            && config_setting_lookup_string(datum, "topic", &_topic)
            && config_setting_lookup_int(datum, "qos", &_qos)
            && config_setting_lookup_float(datum, "factor", &_factor)
        ))
      {
        fprintf(stderr, "Error in config file\n");
        continue;
      }

      // Now store into structure
      
      strcpy(data->topic[j].key, _key);
      strcpy(data->topic[j].topic, _topic);
      data->topic[j].qos = _qos;
      data->topic[j].factor = _factor;

      fprintf(stderr, "Configured topic %s with key %s. QOS = %d factor = %lf\n",
          data->topic[j].topic, data->topic[j].key, data->topic[j].qos, data->topic[j].factor);
      j++;
    }
    data->topics = j;
  }
}

int main(int argc, char* argv[])
{
	int rc;
	int ch;
  char config_file[512];

	homeauto_data data;
	data.mqtt_in.connected = 0;
	data.mqtt_out.connected = 0;
  data.verbose = 0;

  while(1)
  {
		static struct option long_options[] =
		{
			/* These options set a flag. */
			{"verbose",   no_argument, NULL, 'v'},
			{"config",    required_argument, 0, 'c'},
			{0, 0, 0, 0}
		};

    long_options[0].flag = &data.verbose;

		int option_index = 0;
    int c = getopt_long(argc, argv, "vf:", long_options, &option_index);

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
      case 0:
        if(long_options[option_index].flag != 0)
        {
          break;
        }
        break;
      case 'c':
        strncpy(config_file, optarg, 512);
        break;
      case 'v':
        data.verbose = 1;
        break;
      case '?':
        break;
      default:
        abort();
    }
  }

  read_config(&data, config_file);

  MQTTAsync_connectOptions mqtt_conn_opts_out = MQTTAsync_connectOptions_initializer;
  mqtt_conn_opts_out.username = data.mqtt_out.username;
  mqtt_conn_opts_out.password = data.mqtt_out.password;
  mqtt_conn_opts_out.onSuccess = mqtt_out_connect;
	mqtt_conn_opts_out.keepAliveInterval = 20;
  mqtt_conn_opts_out.cleansession = 1;
	mqtt_conn_opts_out.context = &data;

  MQTTAsync_connectOptions mqtt_conn_opts_in = MQTTAsync_connectOptions_initializer;
	mqtt_conn_opts_in.keepAliveInterval = 20;
	mqtt_conn_opts_in.cleansession = 1;
  mqtt_conn_opts_in.onSuccess = mqtt_in_connect;
	mqtt_conn_opts_in.context = &data;

	MQTTAsync_create(&data.mqtt_out.client, data.mqtt_out.server, data.mqtt_out.client_id,
			MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_create(&data.mqtt_in.client, data.mqtt_in.server, data.mqtt_in.client_id,
			MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(data.mqtt_out.client, &data, mqtt_out_connlost, NULL, NULL);
	MQTTAsync_setCallbacks(data.mqtt_in.client, &data, mqtt_in_connlost, mqtt_message_callback, NULL);

  fprintf(stderr, "Connecting to output MQTT .....\n");

  if((rc = MQTTAsync_connect(data.mqtt_out.client, &mqtt_conn_opts_out)) != MQTTASYNC_SUCCESS)
  {
      printf("Failed to connect, return code %d\n", rc);
      exit(EXIT_FAILURE);
  }

  fprintf(stderr, "Connecting to input MQTT .....\n");

	if ((rc = MQTTAsync_connect(data.mqtt_in.client, &mqtt_conn_opts_in)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

  fprintf(stderr, "Starting Busy Loop .....\n");
	while(1)
  {
    pause();
  }
	return rc;
}
