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
		+ index + "/"
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

// --------------------------------------------------------------

// --------------------------------------------------------------

// --------------------------------------------------------------

// --------------------------------------------------------------


