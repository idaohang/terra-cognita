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
#include "mappero.c"

#define pointsPerFrame 10
#define initialPointDiameter 10

struct point {
    int unitx;
    int unity;
    float lat;
    float lon;
    int time;
    float speed;
    ofColor color;
};

vector<point> points;
int *a0 = new int[2];
int *viewCoords = new int[2]; // Will indicate the coordinates of the current viewport, relative to the initial point
bool dragging = false, stopped = false;
int *initCursorPos = new int[2];
int *initViewCoords = new int[2];
float zoom = 1;
unsigned int pathLength = 0;
int pointWidth = initialPointDiameter*zoom;
int pointHeight = pointWidth;
unsigned char * colorAlphaPixels, * colorAlphaPixelsHead;
ofTexture texPoint, texHead;
ofFbo fbo;
ofTrueTypeFont 	speedFont, timeFont;
MapGeo prev_lon = 0, prev_lat = 0;
int prev_t = 0;
int currentSpeed = 0;
float maxSpeed = 0, maxSpeedHalf, maxSpeedQuarter;
ofPoint windowDimensions;

/* Given two points with their latitude and longitude, return the distance between them in Km */
double latlon2distance(MapGeo lat1, MapGeo lon1, MapGeo lat2, MapGeo lon2) {
    double theta, dist;
    theta = lon1 - lon2;
    dist = abs(acos(sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta))))*111.18957696; // 60×1.1515×1.609344
    return dist;
}

void rescalePoint() {
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            int distanceX = abs(j - pointWidth/2);
            int distanceY = abs(i - pointHeight/2);
            float distanceToCenter = sqrt(distanceX*distanceX + distanceY*distanceY);
            float relativeDistanceToCenter = min(float(1), distanceToCenter/(pointWidth/2));
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = 4.5*(1 - relativeDistanceToCenter);
            colorAlphaPixelsHead[(j*pointWidth+i)*4 + 3] = 127*(1 - relativeDistanceToCenter);
        }
    }
}

static int sqliteCallback(void *NotUsed, int argc, char **argv, char **azColName){
    int x = atoi(argv[0]), y = atoi(argv[1]), t = atoi(argv[2]);
    point newPoint;
    /* These coordinates center the visualization on Berlin */
    newPoint.unitx = (x - 288170000)/380;
    newPoint.unity = (y - 175950000)/380;
    /* These coordinates center the visualization on Barcelona
    newPoint.unitx = (atoi(argv[0]) - 271380000)/380;
    newPoint.unity = (atoi(argv[1]) - 200380000)/380; */
    MapGeo lat;
    MapGeo lon;
    unit2latlon_google(x, y, &lat, &lon);
    double distance = latlon2distance(lat, lon, prev_lat, prev_lon);
    int speed = (int)((double)(distance*3600000)/(double)(max(1, t - prev_t)));
    if(strcmp(argv[0], "0") != 0 && newPoint.unitx < windowDimensions.x && newPoint.unity > 0 && newPoint.unity < windowDimensions.y){
        newPoint.time = t;
        newPoint.speed = currentSpeed;
        points.push_back(newPoint);
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

unsigned char * initColorAlphaPixels(int diameter, float brightness) {
    unsigned char * pixels = new unsigned char [diameter*diameter*4];
    for(int i = 0; i < diameter; i++) {
        for(int j = 0; j < diameter; j++) {
            pixels[(j*diameter+i)*4 + 0] = 255;
            pixels[(j*diameter+i)*4 + 1] = 255;
            pixels[(j*diameter+i)*4 + 2] = 255;
            int distanceX = abs(j - diameter/2);
            int distanceY = abs(i - diameter/2);
            float distanceToCenter = sqrt(distanceX*distanceX + distanceY*distanceY);
            float relativeDistanceToCenter = min(float(1), distanceToCenter/(diameter/2));
            pixels[(j*diameter+i)*4 + 3] = brightness*(1 - relativeDistanceToCenter);
        }
    }
    return pixels;
}

void changeColorAlphaPixels(unsigned char * pixels, ofColor color) {
    for(int j = 0; j < pointHeight; j++) {
        for(int k = 0; k < pointWidth; k++) {
            pixels[(k*pointWidth+j)*4 + 0] = color.r;
            pixels[(k*pointWidth+j)*4 + 1] = color.g;
            pixels[(k*pointWidth+j)*4 + 2] = color.b;
        }
    }
}

void drawPoints(unsigned int from, unsigned int to) {
    while(from < to) {
        unsigned int speed = abs(points[from].speed);
        changeColorAlphaPixels(colorAlphaPixels, points[from].color);
        texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
        int X = points[from].unitx*zoom, Y = points[from].unity*zoom;
        texPoint.draw(X, Y, pointWidth, pointHeight);
        from++;
    }
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
    alpha = 0;
	counter = 0;
	cout << "About to load the font";
    cout << "\n";
	//speedFont.loadFont("DejaVuSans-ExtraLight.ttf", 20);
	timeFont.loadFont("TerminusMedium-4.38.ttf", 20);
	cout << "Just loaded the font";
    cout << "\n";

    ofBackground(0,0,0);
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    texPoint.allocate(initialPointDiameter*initialPointDiameter, initialPointDiameter*initialPointDiameter, GL_RGBA);
    texHead.allocate(initialPointDiameter*initialPointDiameter*4, initialPointDiameter*initialPointDiameter*4, GL_RGBA);
	colorAlphaPixelsHead = initColorAlphaPixels(initialPointDiameter*2, 127);
	colorAlphaPixels = initColorAlphaPixels(initialPointDiameter, 4.5);
    texHead.loadData(colorAlphaPixelsHead, initialPointDiameter*2, initialPointDiameter*2, GL_RGBA);
    fbo.allocate(3500, 3500, GL_RGBA);

    viewCoords[0] = 0;
    viewCoords[1] = 0;
    initViewCoords[0] = 0;
    initViewCoords[1] = 0;
	ofEnableAlphaBlending();
    cout << "Calculated maxSpeed: " << maxSpeed;
    maxSpeed = min(maxSpeed, (float)600); // We assume that we never go faster than 600 km/h, anything higher than that is erroneous data
    cout << "maxSpeed: ";
    cout << maxSpeed;
    cout << "\n";
	maxSpeedHalf = maxSpeed/2;
	maxSpeedQuarter = maxSpeedHalf/2;
	cout << "points.size(): ";
	cout << points.size();
	cout << "\n";
	for(int i = 0; i < points.size(); i++) {
        unsigned int R = 0, G = 0, B = 0;
        int speed = min(points[i].speed, maxSpeed);
        if(speed < maxSpeedHalf) {
            R = 0;
            G = 255*(1 - (float)speed/maxSpeedHalf);
            B = 255*((float)speed/maxSpeedHalf);
        }
        else {
            R = 255*((float)(speed - maxSpeedHalf)/maxSpeedHalf);
            G = 0;
            B = 255*(1 - (float)(speed - maxSpeedHalf)/maxSpeedHalf);
        }
        points[i].color.r = R;
        points[i].color.g = G;
        points[i].color.b = B;
	}
}


//--------------------------------------------------------------
void testApp::update(){
    texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
    // Increase the path's length
    if(!dragging && !stopped && pathLength + pointsPerFrame <= points.size()) {
        pathLength += pointsPerFrame;
    }
}

//--------------------------------------------------------------
void testApp::draw(){
    int prevX = points[pathLength - 1].unitx*zoom;
    int prevY = points[pathLength - 1].unity*zoom;
    if(!dragging && !stopped && pathLength + pointsPerFrame <= points.size()) {
        fbo.begin();
        ofSetColor(255, 255, 255);
        drawPoints(max(0, int(pathLength - pointsPerFrame)), pathLength - 1);
        fbo.end();
    }
    ofDisableAlphaBlending();
    fbo.draw(viewCoords[0], viewCoords[1]);
    ofEnableAlphaBlending();
    //sprintf(speedString, "%i km/h", speed);
    //speedFont.drawString(speedString, windowDimensions.x - 420, windowDimensions.y - 40);
    if(pathLength + pointsPerFrame <= points.size()) {
        int X = points[pathLength - 1].unitx*zoom, Y = points[pathLength - 1].unity*zoom;
        if(abs(X - prevX) < 7*zoom && abs(Y - prevY) < 7*zoom) { //Filter out aberrations
            texHead.draw(X+viewCoords[0], Y+viewCoords[1], pointWidth, pointHeight);
        }
        time_t milliseconds = points[pathLength].time;
        tm time = *localtime(&milliseconds);
        strftime(eventTimeString, 100, "%d\/%m\/%Y", &time);
    }
    ofSetHexColor(0x888888);
    timeFont.drawString(eventTimeString, windowDimensions.x - 160, windowDimensions.y - 20);
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
        fbo.begin();
        ofClear(0, 0, 0, 0);
        drawPoints(0, pathLength - pointsPerFrame - 1);
        fbo.end();
        ofDisableAlphaBlending();
        fbo.draw(0, 0);
        ofEnableAlphaBlending();
    }
    else if(key == 45) {
        zoom *= 0.8;
        pointWidth = max(float(2), initialPointDiameter*zoom); // At least 3, so that it doesn't disappear
        pointHeight = pointWidth;
        rescalePoint();
        viewCoords[0] *= 0.8;
        viewCoords[0] *= 0.8;
        fbo.begin();
        ofClear(0, 0, 0, 0);
        drawPoints(0, pathLength - pointsPerFrame - 1);
        fbo.end();
        ofDisableAlphaBlending();
        fbo.draw(0, 0);
        ofEnableAlphaBlending();
    }
    else if(key == 32) {
        stopped = !stopped;
    }
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
    }
    viewCoords[0] = initViewCoords[0] + x - initCursorPos[0]; // Amount of displacement that we should apply to the viewport
    viewCoords[1] = initViewCoords[1] + y - initCursorPos[1];
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
