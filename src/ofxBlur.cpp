#include "ofxBlur.h"


//----------------------------------------
float Gaussian( float _x, float _mean, float _variance )
{
  _x -= _mean;
  return ( 1. / sqrt( TWO_PI * _variance ) ) * exp( -( _x * _x ) / ( 2 * _variance ) );
}

//----------------------------------------
void GaussianRow( int _elements, vector< float >& _row, float _variance = .2 )
{
  _row.resize( _elements );
  
  for( int i = 0; i < _elements; ++i )
  {
    float x   = ofMap( i, 0, _elements - 1, -1, 1 );
    _row[ i ] = Gaussian( x, 0, _variance );
  }
}

//----------------------------------------
string generateBlurSource( int _radius, float _shape )
{
  int rowSize = 2 * _radius + 1;
  
  // generate row
  vector< float > row;
  GaussianRow( rowSize, row, _shape );
  
  // normalize row and coefficients
  vector< float > coefficients;
  float sum = 0;
  for( auto& r : row ) sum += r;
  for( auto& r : row ) r   /= sum;
  
  int center = row.size() / 2;
  coefficients.push_back( row[ center ] );
  
  for( int i = center + 1; i < row.size(); i += 2 )
  {
    float weightSum = row[ i ] + row[ i + 1 ];
    coefficients.push_back( weightSum );
  }
  
  // generate offsets
  vector< float > offsets;
  for( int i = center + 1; i < row.size(); i += 2 )
  {
    int   left            = i - center;
    int   right           = left + 1;
    float leftVal         = row[ i ];
    float rightVal        = row[ i + 1 ];
    float weightSum       = leftVal + rightVal;
    float weightedAverage = ( left * leftVal + right * rightVal ) / weightSum;
    offsets.push_back( weightedAverage );
  }
  
  stringstream src;
  src << "#version 120"                                                              << endl;
  src << "#extension GL_ARB_texture_rectangle : enable"                              << endl;
  src << "uniform sampler2DRect source;"                                             << endl;
  src << "uniform vec2 direction;"                                                   << endl;
  src << "void main(void) {"                                                         << endl;
  src << "  vec2 tc = gl_TexCoord[0].st;"                                            << endl;
  src << "  gl_FragColor = " << coefficients[ 0 ] << " * texture2DRect(source, tc);" << endl;
  
  for( int i = 1; i < coefficients.size(); ++i )
  {
    int curOffset = i - 1;
    src << "  gl_FragColor += " << coefficients[ i ] << " * "                             << endl;
    src << "    (texture2DRect(source, tc - (direction * " << offsets[ i - 1 ] << ")) + " << endl;
    src << "     texture2DRect(source, tc + (direction * " << offsets[ i - 1 ] << ")));"  << endl;
  }
  src << "}"                                                                              << endl;
  
  return src.str();
}

//----------------------------------------
string generateCombineSource( int _passes, float _downsample )
{
  vector< string > combineNames;
  for( int i = 0; i < _passes; ++i ) combineNames.push_back( "s" + ofToString( i ) );
  
  stringstream src;
  src << "#version 120"                                                                     << endl;
  src << "#extension GL_ARB_texture_rectangle : enable"                                     << endl;
  src << "uniform sampler2DRect " << ofJoinString( combineNames, "," ) << ";"               << endl;
  src << "uniform float brightness;"                                                        << endl;
  src << "const float scaleFactor = " << ( ( _downsample == 1 ) ? 1. : _downsample ) << ";" << endl;
  
  src << "void main(void) {"                                                                << endl;
  src << "  vec2 tc = gl_TexCoord[0].st;"                                                   << endl;
  for( int i = 0; i < _passes; ++i )
  {
    src << "  gl_FragColor " << ( i == 0 ? " =" : "+=" );
    src << " texture2DRect(" << combineNames[ i ] << ", tc);" << endl;
    if( i + 1 != _passes ) src << "  tc *= scaleFactor;"      << endl;
  }
  src << "  gl_FragColor *= brightness / " << _passes << ".;" << endl;
  src << "}";
  
  return src.str();
}

//----------------------------------------
ofxBlur::ofxBlur()
:scale( 1 )
,rotation( 0 )
,brightness( 1 )
{
}

//----------------------------------------
void ofxBlur::setup( int _width, int _height, int _radius, float _shape, int _passes, float _downsample, bool _useFloatTexture )
{
  string blurSource = generateBlurSource( _radius, _shape );
  
  if( ofGetLogLevel() == OF_LOG_VERBOSE ) cout << "ofxBlur is loading blur shader:" << endl << blurSource << endl;
  
  blurShader.setupShaderFromSource( GL_FRAGMENT_SHADER, blurSource );
  blurShader.linkProgram();
  
  if( _passes > 1 )
  {
    string combineSource = generateCombineSource( _passes, _downsample );
    
    if( ofGetLogLevel() == OF_LOG_VERBOSE ) cout << "ofxBlur is loading combine shader:" << endl << combineSource << endl;
    
    combineShader.setupShaderFromSource( GL_FRAGMENT_SHADER, combineSource );
    combineShader.linkProgram();
  }
  
  if( _useFloatTexture ) base.allocate( _width, _height, GL_RGB32F );
  else                   base.allocate( _width, _height );
  
  base.begin();
  {
    ofClear( 0 );
  }
  base.end();
  
  ofFbo::Settings settings;
  settings.useDepth   = false;
  settings.useStencil = false;
  settings.numSamples = 0;
  
  ping.resize( _passes );
  pong.resize( _passes );
  
  for( int i = 0; i < _passes; ++i )
  {
    ofLogVerbose() << "building ping/pong " << _width << "x" << _height;
    settings.width  = _width;
    settings.height = _height;
    
    ping[ i ].allocate( settings );
    ping[ i ].begin();
    {
      ofClear( 0 );
    }
    ping[ i ].end();
    
    pong[ i ].allocate( settings );
    pong[ i ].begin();
    {
      ofClear( 0 );
    }
    pong[ i ].end();
    
//    ping[ i ].setDefaultTextureIndex( i );
//    pong[ i ].setDefaultTextureIndex( i );
    
    _width  *= _downsample;
    _height *= _downsample;
  }
}

//----------------------------------------
void ofxBlur::begin()
{
  base.begin();
}

//----------------------------------------
void ofxBlur::end()
{
  base.end();
  
  ofPushStyle();
  {
    ofSetColor( 255 );
    
    ofVec2f xDirection = ofVec2f( scale, 0 ).getRotatedRad( rotation );
    ofVec2f yDirection = ofVec2f( 0, scale ).getRotatedRad( rotation );
    for( int i = 0; i < ping.size(); ++i )
    {
      ofFbo& curPing = ping[ i ];
      ofFbo& curPong = pong[ i ];
      
      // resample previous result into ping
      curPing.begin();
      {
        int w = curPing.getWidth();
        int h = curPing.getHeight();
        
        if( i > 0 ) ping[ i - 1 ].draw( 0, 0, w, h );
        else        base.draw( 0, 0, w, h );
      }
      curPing.end();
      
      // horizontal blur ping into pong
      curPong.begin();
      {
        blurShader.begin();
        {
          blurShader.setUniformTexture( "source", curPing.getTexture(), 0 );
          blurShader.setUniform2f( "direction", xDirection.x, xDirection.y );
          curPing.draw( 0, 0 );
        }
        blurShader.end();
      }
      curPong.end();
      
      // vertical blur pong into ping
      curPing.begin();
      {
        blurShader.begin();
        {
          blurShader.setUniformTexture( "source", curPong.getTexture(), 0 );
          blurShader.setUniform2f( "direction", yDirection.x, yDirection.y );
          curPong.draw( 0, 0 );
        }
        blurShader.end();
      }
      curPing.end();
    }
    
    // render ping back into base
    if( ping.size() > 1 )
    {
      int w = base.getWidth();
      int h = base.getHeight();
      
      ofPlanePrimitive plane;
      plane.set( w, h );
      plane.mapTexCoordsFromTexture( ping[ 0 ].getTexture() );
      
      base.begin();
      {
        combineShader.begin();
        {
          for( int i = 0; i < ping.size(); ++i )
          {
            combineShader.setUniformTexture( "s" + ofToString( i ), ping[ i ].getTexture(), i + 1 );
          }
          combineShader.setUniform1f( "brightness", brightness );
          
          ofPushMatrix();
          {
            ofTranslate( w / 2, h / 2 );
            plane.draw();
          }
          ofPopMatrix();
        }
        combineShader.end();
      }
      base.end();
    }
    else
    {
      base.begin();
      {
        ping[ 0 ].draw( 0, 0 );
      }
      base.end();
    }
  }
  ofPopStyle();
}

//----------------------------------------
ofTexture& ofxBlur::getTextureReference() {
  return getTexture();
}

//----------------------------------------
void ofxBlur::draw( int _x, int _y, int _w, int _h )
{
  base.draw( _x, _y, _w, _h );
}

//----------------------------------------
void ofxBlur::draw( int _x, int _y )
{
  draw( _x, _y, base.getWidth(), base.getHeight() );
}

//----------------------------------------
void ofxBlur::draw()
{
  draw( 0, 0 );
}

//----------------------------------------
void ofxBlur::draw( ofRectangle _rect )
{
  draw( _rect.x, _rect.y, _rect.width, _rect.height );
}
