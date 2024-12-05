#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-unix.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>

#include "ACAP.h"
#include "Model.h"
#include "Video.h"
#include "cJSON.h"
#include "Output.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

#define APP_PACKAGE	"detectx"

cJSON* settings = 0;
cJSON* model = 0;
cJSON* eventsTransition = 0;
cJSON* eventLabelCounter = 0;
GTimer *cleanupTransitionTimer = 0;

void
ConfigUpdate( const char *service, cJSON* data) {
	LOG_TRACE("%s: %s\n",__func__,service);
	cJSON* setting = data->child;
	while(setting) {
		LOG_TRACE("%s: Processing %s\n",__func__,setting->string);
		if( strcmp( "eventTimer", setting->string ) == 0 ) {
			LOG("Changed event state to %d\n", setting->valueint);
		}
		if( strcmp( "eventsTransition", setting->string ) == 0 ) {
			LOG("Changed event transition to %d\n", setting->valueint);
		}
		if( strcmp( "aoi", setting->string ) == 0 ) {
			LOG("Updated area of intrest\n");
		}
		if( strcmp( "ignore", setting->string ) == 0 ) {
			LOG("Update labels to be processed\n");
		}
		if( strcmp( "confidence", setting->string ) == 0 ) {
			LOG("Updated confidence threshold to %d\n", setting->valueint);
		}
		setting = setting->next;
	}
	LOG_TRACE("%s: Exit\n",__func__);
}


VdoMap *capture_VDO_map = NULL;

int inferenceCounter = 0;
unsigned int inferenceAverage = 0;

gboolean
ImageProcess(gpointer data) {
	const char* label = "Undefined";
    struct timeval startTs, endTs;	

	if( !settings || !model )
		return G_SOURCE_REMOVE;

	VdoBuffer* buffer = Video_Capture_YUV();	

	if( !buffer ) {
		ACAP_STATUS_SetString("model","status","Error. Check log");
		ACAP_STATUS_SetBool("model","state", 0);
		LOG_WARN("Image capture failed\n");
		return G_SOURCE_REMOVE;
	}

    gettimeofday(&startTs, NULL);
	cJSON* detections = Model_Inference(buffer);
    gettimeofday(&endTs, NULL);

	unsigned int inferenceTime = (unsigned int)(((endTs.tv_sec - startTs.tv_sec) * 1000) + ((endTs.tv_usec - startTs.tv_usec) / 1000));
	inferenceCounter++;
	inferenceAverage += inferenceTime;
	if( inferenceCounter >= 10 ) {
		ACAP_STATUS_SetNumber(  "model", "averageTime", (int)(inferenceAverage / 10) );
		inferenceCounter = 0;
		inferenceAverage = 0;
	}

	double timestamp = ACAP_DEVICE_Timestamp();

	//Apply Transform detection data and apply user filters
	cJSON* processedDetections = cJSON_CreateArray();
	cJSON* aoi = cJSON_GetObjectItem(settings,"aoi");
	if(!aoi) {
		ACAP_STATUS_SetString("model","status","Error. Check log");
		ACAP_STATUS_SetBool("model","state", 0);
		LOG_WARN("No aoi settings\n");
		return G_SOURCE_REMOVE;
	}
	unsigned int x1 = cJSON_GetObjectItem(aoi,"x1")?cJSON_GetObjectItem(aoi,"x1")->valueint:100;
	unsigned int y1 = cJSON_GetObjectItem(aoi,"y1")?cJSON_GetObjectItem(aoi,"y1")->valueint:100;
	unsigned int x2 = cJSON_GetObjectItem(aoi,"x2")?cJSON_GetObjectItem(aoi,"x2")->valueint:900;
	unsigned int y2 = cJSON_GetObjectItem(aoi,"y2")?cJSON_GetObjectItem(aoi,"y2")->valueint:900;

	cJSON* size = cJSON_GetObjectItem(settings,"size");
	if(!size) {
		ACAP_STATUS_SetString("model","status","Error. Check log");
		ACAP_STATUS_SetBool("model","state", 0);
		LOG_WARN("No size settings\n");
		return G_SOURCE_REMOVE;
	}
	unsigned int minWidth = cJSON_GetObjectItem(size,"x2")->valueint - cJSON_GetObjectItem(size,"x1")->valueint;
	unsigned int minHeight = cJSON_GetObjectItem(size,"y2")->valueint - cJSON_GetObjectItem(size,"y1")->valueint;

	int confidenceThreshold = cJSON_GetObjectItem(settings,"confidence")?cJSON_GetObjectItem(settings,"confidence")->valueint:0.5;
		
	cJSON* detection = detections->child;
	while(detection) {
		cJSON* property = detection->child;
		unsigned cx = 0;
		unsigned cy = 0;
		unsigned width = 0;
		unsigned height = 0;
		unsigned c = 0;
		label = "Undefined";
		while(property) {
			if( strcmp("c",property->string) == 0 ) {
				property->valueint = property->valuedouble * 100;
				property->valuedouble = property->valueint;
				c = property->valueint;
			}
			if( strcmp("x",property->string) == 0 ) {
				property->valueint = property->valuedouble * 1000;
				property->valuedouble = property->valueint;
				cx += property->valueint;
			}
			if( strcmp("y",property->string) == 0 ) {
				property->valueint = property->valuedouble * 1000;
				property->valuedouble = property->valueint;
				cy += property->valueint;
			}
			if( strcmp("w",property->string) == 0 ) {
				property->valueint = property->valuedouble * 1000;
				width = property->valueint;
				property->valuedouble = property->valueint;
				cx += property->valueint / 2;
			}
			if( strcmp("h",property->string) == 0 ) {
				property->valueint = property->valuedouble * 1000;
				height = property->valueint;
				property->valuedouble = property->valueint;
				cy += property->valueint / 2;
			}
			if( strcmp("label",property->string) == 0 ) {
				label = property->valuestring;
			}
			property = property->next;
		}
		

		//FILTER DETECTIONS
		int insert = 0;
		if( c >= confidenceThreshold && cx >= x1 && cx <= x2 && cy >= y1 && cy <= y2 )
			insert = 1;
		if( width < minWidth || height < minHeight )
			insert = 0;
		cJSON* ignore = cJSON_GetObjectItem(settings,"ignore");
		if( insert && ignore && ignore->type == cJSON_Array && cJSON_GetArraySize(ignore) > 0 ) {
			cJSON* ignoreLabel = ignore->child;
			while( ignoreLabel && insert ) {
				if( strcmp( label, ignoreLabel->valuestring) == 0 )
					insert = 0;
				ignoreLabel = ignoreLabel->next;
			}
		}
		//Add custom filter here.  Set "insert = 0" if you want to exclude the detection

		if( insert ) {
			cJSON_AddNumberToObject( detection, "timestamp", timestamp );
			cJSON_AddItemToArray(processedDetections, cJSON_Duplicate(detection,1));
		}
		detection = detection->next;
	}
	
	Output( processedDetections );

	cJSON_Delete(processedDetections);

	return G_SOURCE_CONTINUE;
}

void HTTP_ENDPOINT_eventsTransition(const ACAP_HTTP_Response response,const ACAP_HTTP_Request request) {
	if( !eventsTransition )
		eventsTransition = cJSON_CreateObject();
	ACAP_HTTP_Respond_JSON(  response, eventsTransition);
}

static GMainLoop *main_loop = NULL;

static gboolean
signal_handler(gpointer user_data) {
    LOG("Received SIGTERM, initiating shutdown\n");
    if (main_loop && g_main_loop_is_running(main_loop)) {
        g_main_loop_quit(main_loop);
    }
    return G_SOURCE_REMOVE;
}


int main(void) {
	setbuf(stdout, NULL);
	unsigned int videoWidth = 800;
	unsigned int videoHeight = 600;

	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);

	ACAP( APP_PACKAGE, ConfigUpdate );

	settings = ACAP_Get_Config("settings");
	if(!settings) {
		ACAP_STATUS_SetString("model","status","Error. Check log");
		ACAP_STATUS_SetBool("model","state", 0);
		LOG_WARN("No settings found\n");
		return 1;
	}

	eventLabelCounter = cJSON_CreateObject();

	model = Model_Setup();

	videoWidth = cJSON_GetObjectItem(model,"videoWidth")?cJSON_GetObjectItem(model,"videoWidth")->valueint:800;
	videoHeight = cJSON_GetObjectItem(model,"videoHeight")?cJSON_GetObjectItem(model,"videoHeight")->valueint:600;

	if( model ) {
		ACAP_Set_Config("model", model );
		if( Video_Start_YUV( videoWidth, videoHeight ) ) {
			LOG("Video %ux%u started\n",videoWidth,videoHeight);
		} else {
			LOG_WARN("Video stream for image capture failed\n");
		}
		g_idle_add(ImageProcess, NULL);
	} else {
		LOG_WARN("Model setup failed\n");
	}
	ACAP_Set_Config("model",model);
	Output_reset();
	g_idle_add(ACAP_Process, NULL);
	main_loop = g_main_loop_new(NULL, FALSE);
    GSource *signal_source = g_unix_signal_source_new(SIGTERM);
    if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	} else {
		LOG_WARN("Signal detection failed");
	}

	g_main_loop_run(main_loop);
	LOG("Terminating and cleaning up %s\n",APP_PACKAGE);
	ACAP_Cleanup();
    closelog();	
    return 0;
}
