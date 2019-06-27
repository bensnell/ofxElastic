#pragma once

#include "ofMain.h"
#include "ofxRemoteUIServer.h"
#include "ofxElasticMacros.h"
#include "base64.h"
#include "Poco/UUIDGenerator.h"
#include "Poco/UUID.h"

class ofxElasticDoc {
public:
	string data = "";
	bool bSent = false;

	int httpRequestID = -1; // not sure if this is reliably set in time to make it a useful relational key

	int nSendAttempts = 0;
	float lastSendAttempt = -1;

	bool flagRemove = false;

};

class ElasticResponse {
public:
	string _index = "";
	string _type = "";
	string _id = "";
	int _version = -1;
	string result = "";
	int _shards_total = -1;
	int _shards_successful = -1;
	int _shards_failed = 0;
	int _seq_no = -1;
	int _primary_term = -1;
	
	bool bParsed = false;
	void parse(string data) {
		bParsed = true;
		js = js.parse(data);
		if (js.find("_index") != js.end()) _index = js["_index"];
		if (js.find("_type") != js.end()) _type = js["_type"];
		if (js.find("_id") != js.end()) _id = js["_id"];
		if (js.find("_version") != js.end()) _version = (int)js["_version"];
		if (js.find("result") != js.end()) result = js["result"];
		if (js.find("_shards") != js.end() && js["_shards"].find("total") != js["_shards"].end()) _shards_total = int(js["_shards"]["total"]);
		if (js.find("_shards") != js.end() && js["_shards"].find("successful") != js["_shards"].end()) _shards_successful = int(js["_shards"]["successful"]);
		if (js.find("_shards") != js.end() && js["_shards"].find("failed") != js["_shards"].end()) _shards_failed = int(js["_shards"]["failed"]);
		if (js.find("_seq_no") != js.end()) _seq_no = (int)js["_seq_no"];
		if (js.find("_primary_term") != js.end()) _primary_term = (int)js["_primary_term"];
	}

	string getString() {
		stringstream ss;
		ss << "Elastic Response" << "\n";
		ss << "\t_index:\t\t\t" << _index << "\n";
		ss << "\t_type:\t\t\t" << _type << "\n";
		ss << "\t_id:\t\t\t" << _id << "\n";
		ss << "\t_version:\t\t" << ofToString(_version) << "\n";
		ss << "\tresult:\t\t\t" << result << "\n";
		ss << "\t_shards:\t\t( " << ofToString(_shards_successful) + " succeded + " << ofToString(_shards_failed) << " failed ) / " << ofToString(_shards_total) << " total" << "\n";
		ss << "\t_seq_no:\t\t" << ofToString(_seq_no) << "\n";
		ss << "\t_primary_term:\t" << ofToString(_primary_term);
		return ss.str();
	}

private:
	ofJson js;

};

class ofxElastic : public ofThread {
public:

	// Use the macros instead of these
	static ofxElastic* instance();
	void setupParams();
	void setup();
	void log(string data);

	// Log event data with a template v1
	// If event session is empty, a uuid will be created
	void log_v1(string eventSession, string eventName, string eventData);

	void urlResponse(ofHttpResponse& response);

protected:

	ofxElastic();
	~ofxElastic();

	static ofxElastic* singleton;

	void threadedFunction();

	void update();

	vector<ofxElasticDoc> docQueue;

	ofURLFileLoader loader;
	
	// Params for elastic servers
	string serverUrl = "https://1c89b4484cf14caeb67f532a557d8e59.us-east-1.aws.found.io:9243";
	string index = "abb_test";
	string postType = "_doc"; // always
	// Authentication Params for a Kibana User with appropriate permissions
	string username = "test_user";
	string password = "test_user";
	// Content
	string contentType = "application/json; charset=utf-8";


	// Parameters for sending
	int maxSendAttempts = 5;
	float sendTimeout = 3.0; // seconds

	// Generator utility
	Poco::UUIDGenerator uuidGenerator;

	// Parameters specific to logging templates
	// (index is specified above)
	// If a parameter is empty, it is not recorded
	string organization = "";
	string project = "";
	string exhibit = "";
	string app_name = "";
	string app_version = "";
	float location_geo_lat = 0;
	float location_geo_lon = 0;
	string location_room = "";
	string location_description = "";


	// Helpers for hardware information
	string getComputerModel();
	string getComputerCPU();
	string getComputerGPU();
	string getComputerPlatform();
	string getOS(); // not entirely accurate

};