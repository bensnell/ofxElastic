#pragma once

#ifndef _ofxElasticMacros_
#define _ofxElasticMacros_

#define OFX_ELASTIC_SETUP_PARAMS()							\
( ofxElastic::instance()->setupParams() )

#define OFX_ELASTIC_SETUP()							\
( ofxElastic::instance()->setup() )

#define OFX_ELASTIC_LOG(data, ...)						\
( ofxElastic::instance()->log( data, ##__VA_ARGS__ ) )



#endif // !_ofxElasticMacros_
