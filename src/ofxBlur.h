#pragma once

#include "ofMain.h"

class ofxBlur
{
protected:
  ofFbo           base;
  vector< ofFbo > ping, pong;
  
  ofShader        blurShader, combineShader;
  float           scale;
  float           rotation;
  float           downsample;
  float           brightness;
  
public:
  ofxBlur();
  
  void setup( int _width, int _height, int _radius = 32, float _shape = .2, int _passes = 1, float _downsample = .5, bool _useFloatTexture = false );
  
  void setScale( float _scale ){ scale = _scale; }
  void setRotation( float _rotation ){ rotation = _rotation; }
  void setBrightness( float _brightness ){ brightness = _brightness; } // only applies to multipass
  
  void begin();
  void end();
  
  void draw( int _x, int _y, int _w, int _h );
  void draw( int _x, int _y );
  void draw();
  void draw( ofRectangle _rect );
  
  OF_DEPRECATED_MSG( "Use getTexture", ofTexture& getTextureReference() );
  ofTexture& getTexture(){ return base.getTexture(); }
};

// <3 kyle
