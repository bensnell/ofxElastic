#include "ofxElastic.h"

// --------------------------------------------------------------
ofxElastic::ofxElastic() {

}

// --------------------------------------------------------------
ofxElastic::~ofxElastic() {
	waitForThread(true);
}

// --------------------------------------------------------------
ofxElastic* ofxElastic::singleton = NULL;

// --------------------------------------------------------------
ofxElastic* ofxElastic::instance() {
	if (!singleton) {   // Only allow one instance of class to be generated.
		singleton = new ofxElastic();
	}
	return singleton;
}

// --------------------------------------------------------------
void ofxElastic::threadedFunction() {
	while (isThreadRunning()) {
		update();
		ofSleepMillis(33);
	}
}

// --------------------------------------------------------------
void ofxElastic::setupParams() {

	RUI_NEW_GROUP("ELK_Elastic");
	RUI_SHARE_PARAM_WCN("ELK_Server URL", serverUrl);
	RUI_SHARE_PARAM_WCN("ELK_Index", index);
	RUI_SHARE_PARAM_WCN("ELK_Post Type", postType);
	RUI_SHARE_PARAM_WCN("ELK_Username", username);
	RUI_SHARE_PARAM_WCN("ELK_Password", password);
	RUI_SHARE_PARAM_WCN("ELK_Content Type", contentType);
	RUI_SHARE_PARAM_WCN("ELK_Max Send Attempts", maxSendAttempts, 1, 10);
	RUI_SHARE_PARAM_WCN("ELK_Send Attempt Timeout", sendTimeout, 0.1, 60);

	RUI_NEW_GROUP("ELK_Elastic Template v1");
	RUI_SHARE_PARAM_WCN("ELK_Organization", organization);
	RUI_SHARE_PARAM_WCN("ELK_Project", project);
	RUI_SHARE_PARAM_WCN("ELK_Exhibit", exhibit);
	RUI_SHARE_PARAM_WCN("ELK_App Name", app_name);
	RUI_SHARE_PARAM_WCN("ELK_App Version", app_version);
	RUI_SHARE_PARAM_WCN("ELK_Location Geo Lat", location_geo_lat, -360, 360);
	RUI_SHARE_PARAM_WCN("ELK_Location Geo Lon", location_geo_lon, -360, 360);
	RUI_SHARE_PARAM_WCN("ELK_Location Room", location_room);
	RUI_SHARE_PARAM_WCN("ELK_Location Description", location_description);

}

// --------------------------------------------------------------
void ofxElastic::setup() {
	ofLogNotice("ofxElastic") << "Setting up ofxElastic servers.";

	// Setup the connection, port, ip, etc.
	ofRegisterURLNotification(this);

	// Start the server thread
	startThread();
}

// --------------------------------------------------------------
void ofxElastic::update() {

	// Regenerate the polling location
	string url = serverUrl
		+ (serverUrl.find_last_of('/') == (serverUrl.size() - 1) ? "" : "/")
		+ index + "/" // TODO: make index changeable from doc to doc
		+ postType;
	string headerKey = "Authorization";
	string auth = username + ":" + password;
	string headerValue = "Basic " + base64_encode((unsigned char*)auth.c_str(), auth.length());

	// Remove any docs flagged for removal
	for (int i = docQueue.size() - 1; i >= 0; i--) {
		if (docQueue[i].flagRemove) {
			ofLogVerbose("ofxElastic") << "Deleting doc";
			lock();
			docQueue.erase(docQueue.begin() + i);
			unlock();
		}
	}

	// Go through all documents
	for (int i = 0; i < docQueue.size(); i++) {

		// Get this document
		lock();
		ofxElasticDoc doc = docQueue.front();
		unlock();

		// Check to see if it's time to re-request it
		// If not, then skip this document
		if (doc.lastSendAttempt != 0 && ofGetElapsedTimef() - doc.lastSendAttempt < sendTimeout) continue;

		// This is a document that we need to send
		// Send this document
		ofHttpRequest req;
		req.method = ofHttpRequest::POST;
		req.url = url;
		req.name = "elastic"; // arbitrary
		req.saveTo = false;
		req.headers[headerKey] = headerValue;
		req.body = doc.data;
		req.contentType = contentType;
		//req.done;

		// Nonasync
		//ofHttpResponse resp = loader.handleRequest(req);
		//ofLogNotice("ofxElastic") << "Response status: " << resp.status << "\t error: " << resp.error << "\tdata: " << resp.data.getText();

		// Async
		int requestID = loader.handleRequestAsync(req);
		//ofLogNotice("ofxElastic") << "Async request ID: " << requestID;

		// Save this request ID and mark doc as sending
		lock();
		if (!docQueue.empty()) {
			docQueue.front().httpRequestID = requestID;
			docQueue.front().nSendAttempts++;
			docQueue.front().lastSendAttempt = ofGetElapsedTimef();
		}
		unlock();
	}
}

// --------------------------------------------------------------
void ofxElastic::log(string data) {
	ofLogNotice("ofxElastic") << "Logging Event with data: " << data;

	// Create a document
	ofxElasticDoc doc;
	doc.data = data;

	// Add this doc to the queue
	lock();
	docQueue.push_back(doc);
	unlock();
}

// --------------------------------------------------------------
void ofxElastic::urlResponse(ofHttpResponse& response) {
	if (response.status/100 == 2) {
		ofLogNotice("ofxElastic") << "POST Success: " << response.status << "\t name: " << response.request.name << "\t error: " << response.error << "\tdata: " << response.data.getText();
		ElasticResponse resp;
		resp.parse(response.data.getText());
		ofLogNotice("ofxElastic") << "\n" << resp.getString();

		// Mark this doc for deletion
		for (int i = 0; i < docQueue.size(); i++) {
			if (docQueue[i].httpRequestID == response.request.getID()) {
				ofLogVerbose("ofxElastic") << "successful response, so marking for deletion";
				lock();
				docQueue[i].flagRemove = true;
				unlock();
			}
		}

	}
	else {
		ofLogNotice("ofxElastic") << "POST Failure: " << response.status << "\t name: " << response.request.name << "\t error: " << response.error << "\tdata: " << response.data.getText();

		// Mark this doc for deletion if it has been attempted more than 
		// the max number of times
		// TODO: lock whole iteration?
		for (int i = 0; i < docQueue.size(); i++) {
			if (docQueue[i].httpRequestID == response.request.getID() && docQueue[i].nSendAttempts >= maxSendAttempts) {
				ofLogVerbose("ofxElastic") << "Unsuccessful response and max number of attempts achieved. Marking doc for deletion";
				lock();
				docQueue[i].flagRemove = true;
				unlock();
			}
		}

		// status of -1 for failed
		// by default tries 5 times
		loader.remove(response.request.getID());
	}
}

// --------------------------------------------------------------
void ofxElastic::log_v1(string eventSession, string eventName, string eventData) {

	// Create a unique session if one is not provided
	if (eventSession.empty()) eventSession = uuidGenerator.createRandom().toString();

	// Populate a document with a specific template
	// The template contains this information:
	ofJson js;
	js["time"] = ofGetTimestampString("%Y-%m-%dT%H:%M:%S.%i%z");
	if (!organization.empty()) js["organization"] = organization;
	if (!project.empty()) js["project"] = project;
	ofJson machine;
	machine["type"] = "PC";
	machine["os"] = getOS();
	machine["version"] = "10"; // TODO
	js["machine"] = machine;
	if (!exhibit.empty()) js["exhibit"] = exhibit;
	ofJson app;
	if (!app_name.empty()) app["name"] = app_name;
	if (!app_version.empty()) app["version"] = app_version;
	if (!(app_name.empty() && app_version.empty())) js["app"] = app;
	ofJson location;
	ofJson geo;
	if (location_geo_lat != 0) geo["lat"] = location_geo_lat;
	if (location_geo_lon != 0) geo["lon"] = location_geo_lon;
	if (location_geo_lat != 0 && location_geo_lon != 0) {
		location["geo"] = geo;
	}
	if (!location_room.empty()) location["room"] = location_room;
	if (!location_description.empty()) location["description"] = location_description;
	if (location_geo_lat != 0 || location_geo_lon != 0 || !location_room.empty() || !location_description.empty()) {
		js["location"] = location;
	}
	ofJson evt; // event
	evt["session"] = eventSession;
	evt["name"] = eventName;
	evt["data"] = ofJson::parse(eventData);
	js["event"] = evt;

	// Create a document
	ofxElasticDoc doc;
	doc.data = js.dump();

	ofLogNotice("ofxElastic") << "Logging Event with data: " << doc.data;

	// Add this doc to the queue
	lock();
	docQueue.push_back(doc);
	unlock();
}

// --------------------------------------------------------------
string ofxElastic::getComputerModel() {

#ifdef TARGET_OSX
	size_t len = 0;
	::sysctlbyname("hw.model", NULL, &len, NULL, 0);
	std::string model(len - 1, '\0');
	::sysctlbyname("hw.model", const_cast<char*>(model.data()), &len, NULL, 0);
	return model;
#endif
#ifdef TARGET_WIN32
	return "Windows PC";
#endif
	return "Unknown Model";
}

// --------------------------------------------------------------
string ofxElastic::getComputerCPU() {
#ifdef TARGET_OSX
	size_t len = 0;
	::sysctlbyname("machdep.cpu.brand_string", NULL, &len, NULL, 0);
	std::string cpu(len - 1, '\0');
	::sysctlbyname("machdep.cpu.brand_string", const_cast<char*>(cpu.data()), &len, NULL, 0);
	return cpu;
#endif

#ifdef TARGET_WIN32
	char buf[48];
	int result[4];

	__cpuid(result, 0x80000000);

	if (result[0] >= (int)0x80000004) {
		__cpuid((int*)(buf + 0), 0x80000002);
		__cpuid((int*)(buf + 16), 0x80000003);
		__cpuid((int*)(buf + 32), 0x80000004);

		string brand = buf;

		size_t i;
		if ((i = brand.find("  ")) != string::npos)
			brand = brand.substr(0, i);

		return brand;
	}
#endif
	return "Unknown CPU";
}

// --------------------------------------------------------------
string ofxElastic::getComputerGPU() {
	string renderer = string((char*)glGetString(GL_RENDERER));
	return renderer;
}

// --------------------------------------------------------------
string ofxElastic::getComputerPlatform() {

	ofTargetPlatform platform = ofGetTargetPlatform();
	switch (platform) {
	case OF_TARGET_OSX: {
#ifdef TARGET_OSX
		string platS;
		SInt32 major = 10, minor = 4, bugfix = 1;
		Gestalt(gestaltSystemVersionBugFix, &bugfix);
		Gestalt(gestaltSystemVersionMajor, &major);
		Gestalt(gestaltSystemVersionMinor, &minor);
		platS = "Macintosh; Mac OS X " + ofToString(major) + "." +
			ofToString(minor) + "." + ofToString(bugfix);
		return platS;
#endif
	}break;
	case OF_TARGET_MINGW: return "Windows; MINGW"; break;
	case OF_TARGET_WINVS:  return "Windows; Visual Studio"; break;
	case OF_TARGET_IOS: return "iOS"; break;
	case OF_TARGET_ANDROID: return "Android"; break;
	case OF_TARGET_LINUX: return "Linux"; break;
	case OF_TARGET_LINUX64: return "Linux 64"; break;
	case OF_TARGET_LINUXARMV6L: return "Linux ARM v6"; break;
	case OF_TARGET_LINUXARMV7L: return "Linux ARM v7"; break;
	default: break;
	}
	return "Unknown Platform";
}

// --------------------------------------------------------------
string ofxElastic::getOS() {

	ofTargetPlatform platform = ofGetTargetPlatform();
	switch (platform) {
	case OF_TARGET_OSX: return "Macintosh";  break;
	case OF_TARGET_MINGW: case OF_TARGET_WINVS:  return "Windows"; break;
	case OF_TARGET_IOS: return "iOS"; break;
	case OF_TARGET_ANDROID: return "Android"; break;
	case OF_TARGET_LINUX: case OF_TARGET_LINUX64: case OF_TARGET_LINUXARMV6L: case OF_TARGET_LINUXARMV7L: return "Linux"; break;
	default: break;
	}
	return "Unknown";
}

// --------------------------------------------------------------


