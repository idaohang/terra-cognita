/*
 * Copyright (C) 2012 Joan Perals Tresserra
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "testApp.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>

/* START Code copied from Mappero */

#define deg2rad(deg) ((deg) * (PI / 180.0))
#define rad2deg(rad) ((rad) * (180.0 / PI))

#ifdef USE_DOUBLES_FOR_LATLON
#define GSIN(x) sin(x)
#define GCOS(x) cos(x)
#define GASIN(x) asin(x)
#define GTAN(x) tan(x)
#define GATAN(x) atan(x)
#define GATAN2(x, y) atan2(x, y)
#define GEXP(x) exp(x)
#define GLOG(x) log(x)
#define GPOW(x, y) pow(x, y)
#define GSQTR(x) sqrt(x)
#else
#define GSIN(x) sinf(x)
#define GCOS(x) cosf(x)
#define GASIN(x) asinf(x)
#define GTAN(x) tanf(x)
#define GATAN(x) atanf(x)
#define GATAN2(x, y) atan2f(x, y)
#define GEXP(x) expf(x)
#define GLOG(x) logf(x)
#define GPOW(x, y) powf(x, y)
#define GSQTR(x) sqrtf(x)
#endif

#define MAX_ZOOM (20)
#define TILE_SIZE_P2 (8)

#define WORLD_SIZE_UNITS (2 << (MAX_ZOOM + TILE_SIZE_P2))

#define MERCATOR_SPAN (-6.28318377773622)
#define MERCATOR_TOP (3.14159188886811)

#ifdef USE_DOUBLES_FOR_LATLON
typedef gdouble MapGeo;
#else
typedef gfloat MapGeo;
#endif

/* END Code copied from Mappero */

struct point {
    int unitx;
    int unity;
    float lat;
    float lon;
    int time;
    float speed;
};

vector<point> points;
int *a0 = new int[2];
int *viewCoords = new int[2]; // Will indicate the coordinates of the current viewport, relative to the initial point
bool dragging;
int *initCursorPos = new int[2];
int *initViewCoords = new int[2];
int colorChangeStep;
float zoom = 1;
unsigned int pathLength = 0;
int initialPointDiameter = 20;
int pointWidth = initialPointDiameter*zoom;
int pointHeight = pointWidth;
unsigned char 	* colorAlphaPixels = new unsigned char [400*400*4];
ofTexture texPoint;
ofFbo fbo;
ofTrueTypeFont 	speedFont, timeFont;
MapGeo prev_lon = 0, prev_lat = 0;
int prev_t = 0;
int currentSpeed = 0, maxSpeed = 0, maxSpeedHalf, maxSpeedQuarter;
ofPoint windowDimensions;

/* START Code copied from Mappero */

void unit2latlon_google(gint unitx, gint unity, MapGeo *lat, MapGeo *lon)
{
    MapGeo tmp;
    *lon = (unitx * (360.0 / WORLD_SIZE_UNITS)) - 180.0;
    tmp = (unity * (MERCATOR_SPAN / WORLD_SIZE_UNITS)) + MERCATOR_TOP;
    *lat = (360.0 * (GATAN(GEXP(tmp)))) * (1.0 / PI) - 90.0;
}

void latlon2unit_google(MapGeo lat, MapGeo lon, gint *unitx, gint *unity)
{
    MapGeo tmp;

    *unitx = (lon + 180.0) * (WORLD_SIZE_UNITS / 360.0) + 0.5;
    tmp = GSIN(deg2rad(lat));
    *unity = 0.5 + (WORLD_SIZE_UNITS / MERCATOR_SPAN) *
        (GLOG((1.0 + tmp) / (1.0 - tmp)) * 0.5 - MERCATOR_TOP);
}

/* END Code copied from Mappero */

/* Given two points with their latitude and longitude, return the distance between them in Km */
double latlon2distance(MapGeo lat1, MapGeo lon1, MapGeo lat2, MapGeo lon2) {
    double theta, dist;
    theta = lon1 - lon2;
    dist = abs(acos(sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta))))*111.18957696; // 60×1.1515×1.609344
/*    cout << "dist: ";
    cout << dist;
    cout << "\n";*/
    return dist;
}

void rescalePoint() {
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            int distanceX = abs(j - pointWidth/2);
            int distanceY = abs(i - pointHeight/2);
            float distanceToCenter = sqrt(distanceX*distanceX + distanceY*distanceY);
            float relativeDistanceToCenter = min(float(1), distanceToCenter/(pointWidth/2));
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = 10*(1 - relativeDistanceToCenter);
        }
    }
}

static int sqliteCallback(void *NotUsed, int argc, char **argv, char **azColName){
    int x = atoi(argv[0]), y = atoi(argv[1]), t = atoi(argv[2]);
    //int speed = abs((int)(sqrt((float)(x*x + y*y)/(float)(t - prev_t))));
    point newPoint;
    /* These coordinates are for Berlin */
    newPoint.unitx = (x - 288170000)/380;
    newPoint.unity = (y - 175950000)/380;
    /* These coordinates are for Barcelona
    newPoint.unitx = (atoi(argv[0]) - 271380000)/250;
    newPoint.unity = (atoi(argv[1]) - 200380000)/250; */
    MapGeo lat;
    MapGeo lon;
    unit2latlon_google(x, y, &lat, &lon);
/*    cout << "t: ";
    cout << t;
    cout << "\nprev_t: ";
    cout << prev_t;
    cout << '\n';*/
    double distance = latlon2distance(lat, lon, prev_lat, prev_lon);
    int speed = (int)((double)(distance*3600000)/(double)(max(1, t - prev_t)));
/*    cout << "t - prev_t: ";
    cout << t - prev_t;
    cout << "\n";*/
/*    cout << "speed: ";
    cout << speed;
    cout << " km/h\n";*/
    if(strcmp(argv[0], "0") != 0 && newPoint.unitx < windowDimensions.x && newPoint.unity > 0 && newPoint.unity < windowDimensions.y){
        newPoint.time = t;
        newPoint.speed = currentSpeed;
        points.push_back(newPoint);
/*        cout << argv[0];
        cout << ", ";
        cout << argv[2];
        cout << "\n";*/
        if(speed >= 0) {
            currentSpeed = speed;
        }
    }
    if(maxSpeed < currentSpeed) {
        maxSpeed = currentSpeed;
    }
    prev_lon = lon, prev_lat = lat, prev_t = t;
    return 0;
}

//--------------------------------------------------------------
void testApp::setup(){
	windowDimensions.x = ofGetScreenWidth();
	windowDimensions.y = ofGetScreenHeight();
    int rc;
    cout << "The program is about to start\n";
    sqlite3 *pathsdb; // "Paths" Database Handler
    rc = sqlite3_open("data/paths.db", &pathsdb);
    cout << rc;
    cout << "\n";
    char *error_msg = NULL;
    cout << "SQLite query about to be executed\n";
    rc = sqlite3_exec(pathsdb, "select unitx, unity, time from track_path", sqliteCallback, NULL, &error_msg);
    cout << rc;
    cout << "\n";
    sqlite3_close(pathsdb);
    cout << "maxSpeed: ";
    cout << maxSpeed;
    maxSpeed = min(maxSpeed, 650); // We assume that we never go faster than 650 km/h, anything higher than that is erroneous data
    cout << "\n";
    alpha = 0;
	counter = 0;
	cout << "About to load the font";
    cout << "\n";
	speedFont.loadFont("DejaVuSans-ExtraLight.ttf", 20);
	timeFont.loadFont("DejaVuSans-ExtraLight.ttf", 30);
	cout << "Just loaded the font";
    cout << "\n";

    ofBackground(0,0,0);
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    texPoint.allocate(400, 400, GL_RGBA);
    fbo.allocate(3500, 3500, GL_RGBA);
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 0] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 1] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 2] = 255;
            int distanceX = abs(j - pointWidth/2);
            int distanceY = abs(i - pointHeight/2);
            float distanceToCenter = sqrt(distanceX*distanceX + distanceY*distanceY);
            float relativeDistanceToCenter = min(float(1), distanceToCenter/(pointWidth/2));
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = 4.5*(1 - relativeDistanceToCenter);
        }
    }
    texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);

    viewCoords[0] = 0;
    viewCoords[1] = 0;
    initViewCoords[0] = 0;
    initViewCoords[1] = 0;
    dragging = false;
	ofEnableAlphaBlending();
	colorChangeStep = 1;
	maxSpeedHalf = maxSpeed/2;
	maxSpeedQuarter = maxSpeedHalf/2;
}


//--------------------------------------------------------------
void testApp::update(){
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 0] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 1] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 2] = 255;
        }
    }
    texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
    // Reset the color of the path's texture
	colorChangeStep = max(10,int(pathLength/256));
/*    cout << "colorChangeStep: ";
	cout << colorChangeStep;
	cout << "\n";*/
    // Increase the path's length
    if(pathLength < points.size() - 40) {
        pathLength += 40;
    }
}

//--------------------------------------------------------------
void testApp::draw(){
    int prevX = points[pathLength - 1].unitx*zoom;
    int prevY = points[pathLength - 1].unity*zoom;
    int speed;
    fbo.begin();
    ofSetColor(255, 255, 255);
    for(unsigned int i=max(0, int(pathLength - 40)); i<pathLength; i++) {
        speed = abs(points[i].speed);
        /*cout << "speed: ";
        cout << speed;
        cout << "\n";
        cout << "maxSpeed: ";
        cout << maxSpeed;
        cout << "\n";*/
        int R = 0, G = 0, B = 0;
        if(speed < maxSpeedHalf) {
            R = 0;
            G = 255*(1 - (float)speed/maxSpeedHalf);
            B = 255*((float)speed/maxSpeedHalf);
        }
        else {
            R = 255*(1 - (float)speed/maxSpeedHalf);
            G = 0;
            B = 255*((float)(speed - maxSpeedHalf)/maxSpeedHalf);
        }
/*        int G = (int)(255*pow(cos((float)(speed)/(float)(maxSpeed)),3));
        int B = 255 - G;//(int)(255*pow((float)((float)(speed)/(float)(maxSpeed)), 0.7));
        int R = (int)((int)(255+B)/2);//(int)(255*pow(sin((float)((float)(speed*2)/(float)(maxSpeed))), 0.7));*/
/*        cout << R;
        cout << ", ";
        cout << G;
        cout << ", ";
        cout << B;
        cout << "\n";*/
        for(int i = 0; i < pointHeight; i++) {
            for(int j = 0; j < pointWidth; j++) {
                colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3];
                colorAlphaPixels[(j*pointWidth+i)*4 + 0] = R;
                colorAlphaPixels[(j*pointWidth+i)*4 + 1] = G;
                colorAlphaPixels[(j*pointWidth+i)*4 + 2] = B;
            }
        }
        texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
        int X = points[i].unitx*zoom, Y = points[i].unity*zoom;
        texPoint.draw(X+viewCoords[0], Y+viewCoords[1], pointWidth, pointHeight);
        prevX = X;
        prevY = Y;
    }
    fbo.end();
    ofDisableAlphaBlending();
    fbo.draw(0, 0);
    ofEnableAlphaBlending();
    ofSetHexColor(0xffffff);
    sprintf(speedString, "%d km/h", speed);
    speedFont.drawString(speedString, windowDimensions.x - 420, windowDimensions.y - 40);
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 0] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 1] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 2] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3]*20;
        }
    }
    texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
    int X = points[pathLength - 1].unitx*zoom, Y = points[pathLength - 1].unity*zoom;
    if(abs(X - prevX) < 7*zoom && abs(Y - prevY) < 7*zoom) { //Filter out aberrations
        texPoint.draw(X+viewCoords[0], Y+viewCoords[1], pointWidth, pointHeight);
    }
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3]/20;
        }
    }
    time_t milliseconds = points[pathLength].time;
    tm time = *localtime(&milliseconds);
    strftime(eventTimeString, 100, "%d\/%m\/%Y", &time);
    timeFont.drawString(eventTimeString, windowDimensions.x - 255, windowDimensions.y - 40);
}


//--------------------------------------------------------------
void testApp::keyPressed  (int key){

}

//--------------------------------------------------------------
void testApp::keyReleased(int key){
    if(key == 43) {
        zoom *= 1.2;
        pointWidth = max(float(2), initialPointDiameter*zoom);
        pointHeight = pointWidth;
        rescalePoint();
        viewCoords[0] *= 1.2;
        viewCoords[1] *= 1.2;
    }
    else if(key == 45) {
        zoom *= 0.8;
        pointWidth = max(float(2), initialPointDiameter*zoom); // At least 3, so that it doesn't disappear
        pointHeight = pointWidth;
        rescalePoint();
        viewCoords[0] *= 0.8;
        viewCoords[0] *= 0.8;
    }
/*    cout << "zoom: ";
    cout << zoom;
    cout << "\n";*/
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){
    if(dragging == false) {
        initCursorPos[0] = x;
        initCursorPos[1] = y;
        dragging = true;
/*        cout << "\n";
        cout << initCursorPos[0];
        cout << ", ";
        cout << initCursorPos[1];
        cout << "\n\n";*/
    }
    viewCoords[0] = initViewCoords[0] + x - initCursorPos[0]; // Amount of displacement that we should apply to the viewport
    viewCoords[1] = initViewCoords[1] + y - initCursorPos[1];
/*    cout << viewCoords[0];
    cout << ", ";
    cout << viewCoords[1];
    cout << "\n";*/
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
    dragging = false;
    initViewCoords[0] = viewCoords[0];
    initViewCoords[1] = viewCoords[1];
}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){

}
