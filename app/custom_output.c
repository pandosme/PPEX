/*
 * DetectX will automatically fire events "label" and "counter".
 * When using a custom model it is possible to have custom logic and output
 * upon detections.
 *
 * The detectionList is an array of pre-processed and filtered detections.
 * JSON Syntax:
 * [
 *		{
 *			"label":"some label",		The label od the detected object
 *			"c":82,						The confidence value between 0-100  
 *			"x":100,					The top left corner [0-1000]
 *			"y":100,					The top left corner [0-1000]
 *			"w":100,					The object width [0-1000] 
 *			"h":100,					The object height [0-1000]
 *			"timestamp":1731531483123	//EPOCH timestam since Jan 1 1970. millisecond resolution
 *		},
 *		....
 *	]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "ACAP.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

int *personCounter = 0;
int *helmetCounter = 0;
int *vestCounter = 0;
int counterIndex = 0;
int counterSize = 5;
int helmetState = 0;
int vestState = 0;

void
custom_output( cJSON* detectionList ) {
	LOG_TRACE("%s:\n",__func__);
	if( cJSON_GetArraySize( detectionList ) == 0 ) {
		LOG_TRACE("%s: Exit no detections\n",__func__);
		return;
	}

	if( !personCounter || !helmetCounter || !vestCounter ) {
		LOG_TRACE("%s: Exit Counter not initialized\n",__func__);
		return;
	}

	cJSON* detection = detectionList->child;
	while( detection ) {
		const char* label = cJSON_GetObjectItem( detection,"label")->valuestring;
		if( strcmp("Person",label) == 0 )
			personCounter[counterIndex]++;
		if( strcmp("Helmet",label) == 0 )
			helmetCounter[counterIndex]++;
		if( strcmp("Vest",label) == 0 )
			vestCounter[counterIndex]++;
		detection = detection->next;
	}
	counterIndex++;
	if( counterIndex >= counterSize )
		counterIndex = 0;

	int persons = 0;
	int helmets = 0;	
	int vests = 0;	
	int i;
	for( i = 0; i < counterSize; i++ ) {
		persons += helmetCounter[i];
		helmets += helmetCounter[i];
		vests += vestCounter[i];
	}

	persons = (persons + 1) / counterSize;
	helmets = (helmets + 1) / counterSize;
	vests = (vests + 1) / counterSize;

	if( persons == 0 ) {
		if( helmetState ) {
			ACAP_EVENTS_Fire_State("NoHelmet", 0);
			helmetState = 0;
		}
		if( vestState ) {
			ACAP_EVENTS_Fire_State("NoVest", 0);
			vestState = 0;
		}
		LOG_TRACE("%s: Exit No persons\n",__func__);
		return;
	}

	if( helmetState ) {
		if( helmets >= persons ) {
			helmetState = 0;
			ACAP_EVENTS_Fire_State("NoHelmet", 0);
		}
	} else {
		if( helmets < persons ) {
			helmetState = 1;
			ACAP_EVENTS_Fire_State("NoHelmet", 1);
		}
	}

	if( vestState ) {
		if( vests >= persons ) {
			vestState = 0;
			ACAP_EVENTS_Fire_State("NoVest", 0);
		}
	} else {
		if( vests < persons ) {
			vestState = 1;
			ACAP_EVENTS_Fire_State("NoVest", 1);
		}
	}
	LOG_TRACE("%s: Exit\n",__func__);
}

void
custom_output_reset() {
	LOG_TRACE("%s:\n",__func__);

	cJSON* settings = ACAP_Get_Config("settings");
	if( !settings ) {
		LOG_WARN("%s: No settings defined\n",__func__);
		return;
	}

	if( personCounter ) free( personCounter );
	if( helmetCounter ) free( helmetCounter );
	if( vestCounter ) free( vestCounter );

	counterSize = cJSON_GetObjectItem(settings,"transitionSpeed")?cJSON_GetObjectItem(settings,"transitionSpeed")->valueint:5;
	if( counterSize > 50 )
		counterSize = 50;
	if( counterSize < 5 )
		counterSize = 5;


	personCounter = (int *)calloc(counterSize, sizeof(int));
	helmetCounter = (int *)calloc(counterSize, sizeof(int));
	vestCounter = (int *)calloc(counterSize, sizeof(int));


	counterIndex = 0;
	helmetState = 0;
	vestState = 0;
	ACAP_EVENTS_Fire_State("NoHelmet",0);
	ACAP_EVENTS_Fire_State("NoVest",0);	
	LOG_TRACE("%s: Exit\n",__func__);
}
