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
#include <time.h>
#include "mqtt_bridge.h"

unsigned long watchdog;

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
  char data_buffer[MQTT_BRIDGE_MAX_STRLEN];
  snprintf(data_buffer, MQTT_BRIDGE_MAX_STRLEN, 
      "{\"ts\":%u000, \"values\":{\"%s\":%lf}}", ts, key, value);

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
  
  MQTTAsync_sendMessage(data->mqtt_out.client, MQTT_BRIDGE_TB_TOPIC, &pubmsg, &opts);
}

int mqtt_out_message_callback(void *context, char *topic_name, int topic_len, MQTTAsync_message *message)
{
    homeauto_data *data = (homeauto_data*)context;

    char method[128];
    char char_param[128];
    int int_param;
    int i = sscanf(message->payload, "{\"method\":\"%[^\"]\",\"params\":\"%d\"}", &method, &int_param);
    if(i == 2)
    {
      fprintf(stderr, "i = %d, method = %s, int param = %d\n", i, method, int_param);
    } else if(i == 1) {
      i = sscanf(message->payload, "{\"method\":\"%[^\"]\",\"params\":%[^}]}", &method, &char_param);
      fprintf(stderr, "i = %d, method = %s, params = %s\n", i, method, char_param);
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic_name);
    return 1;
}

int mqtt_in_message_callback(void *context, char *topic_name, int topic_len, MQTTAsync_message *message)
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

    watchdog = (unsigned long)time(NULL);
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
      fprintf(stderr, "Failed to subscribe to %s, return code %d\n", data->topic[i].topic, rc);
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

  int rc;
  const char* topic = "v1/devices/me/rpc/request/+";
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	opts.context = data;
  if((rc = MQTTAsync_subscribe(data->mqtt_out.client, topic, 1,
          &opts)) != MQTTASYNC_SUCCESS)
  {
    fprintf(stderr, "Failed to subscribe to , return code %d\n", topic, rc);
  } else {
    fprintf(stderr, "Subscribed to %s\n", topic);
  }

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
    goto error; 
  }

  if(!read_config_server(&cfg, &data->mqtt_in, "server_in"))
  {
    goto error;
  }

  if(!read_config_server(&cfg, &data->mqtt_out, "server_out") )
  {
    goto error;
  }

  setting = config_lookup(&cfg, "telemetry_data");
  if(setting == NULL)
  {
    fprintf(stderr, "Could not find telemetry data in config file....\n");
    goto error;
  }

  unsigned int count = config_setting_length(setting);
  if(count > MQTT_BRIDGE_MAX_TOPICS)
  {
    fprintf(stderr, "Config file contains too many topics/....\n");
    goto error;
  }
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
      fprintf(stderr, "Error in config file reading topic\n");
      goto error;
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
  config_destroy(&cfg);
  return 1;

error:
  config_destroy(&cfg);
  return 0;
}

int main(int argc, char* argv[])
{
	int rc;
	int ch;
  char config_file[512];

  // Start by setting some defualts

	homeauto_data data;
	data.mqtt_in.connected = 0;
	data.mqtt_out.connected = 0;
  data.verbose = 0;

  // Parse the command line

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
        return EXIT_FAILURE;
    }
  }

  if(!read_config(&data, config_file))
  {
    fprintf(stderr, "Unable to read config file ... exiting ...\n");
    return EXIT_FAILURE;
  }

  // Setup the connection options 

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

  // Create the MQTT Async Objects

	MQTTAsync_create(&data.mqtt_out.client, data.mqtt_out.server, data.mqtt_out.client_id,
			MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_create(&data.mqtt_in.client, data.mqtt_in.server, data.mqtt_in.client_id,
			MQTTCLIENT_PERSISTENCE_NONE, NULL);

  // Set the nessesary callbacks

	MQTTAsync_setCallbacks(data.mqtt_out.client, &data, mqtt_out_connlost, mqtt_out_message_callback, NULL);
	MQTTAsync_setCallbacks(data.mqtt_in.client, &data, mqtt_in_connlost, mqtt_in_message_callback, NULL);

  fprintf(stderr, "Connecting to output MQTT .....\n");

  if((rc = MQTTAsync_connect(data.mqtt_out.client, &mqtt_conn_opts_out)) != MQTTASYNC_SUCCESS)
  {
      printf("Failed to connect, return code %d\n", rc);
      return EXIT_FAILURE;
  }

  fprintf(stderr, "Connecting to input MQTT .....\n");

	if ((rc = MQTTAsync_connect(data.mqtt_in.client, &mqtt_conn_opts_in)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to connect, return code %d\n", rc);
		return EXIT_FAILURE;
	}

  fprintf(stderr, "Starting Busy Loop .....\n");
  watchdog = (unsigned long)time(NULL);
	while(1)
  {
    sleep(5);
    unsigned long diff = (unsigned long)time(NULL) - watchdog;
    if(diff > 100)
    {
      fprintf(stderr, "Exiting due to watchdog : timer = %ld\n", diff);
      return EXIT_FAILURE;
    }
  }

	return EXIT_SUCCESS;
}
