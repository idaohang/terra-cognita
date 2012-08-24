#include "testApp.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>

vector<int*> points;
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
ofTrueTypeFont 	timeFont;
int prev_x = 0, prev_y = 0, prev_t = 0;
int currentSpeed = 0, maxSpeed = 0, maxSpeedHalf, maxSpeedQuarter;
ofPoint windowDimensions;

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

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    int x = atoi(argv[0]), y = atoi(argv[1]), t = atoi(argv[2]);
    int speed = abs((int)(sqrt((float)(x*x + y*y)/(float)(t - prev_t))));
    int *a = new int[2];
    /* These coordinates are for Berlin */
    a[0] = (x - 288170000)/380;
    a[1] = (y - 175950000)/380;
    /* These coordinates are for Barcelona
    a[0] = (atoi(argv[0]) - 271380000)/250;
    a[1] = (atoi(argv[1]) - 200380000)/250; */
    a[2] = t;
    if(strcmp(argv[0], "0") != 0 && a[0] < windowDimensions.x && a[1] > 0 && a[1] < windowDimensions.y){
        points.push_back(a);
/*        cout << argv[0];
        cout << ", ";
        cout << argv[2];
        cout << "\n";*/
        if(speed >= 0) {
            currentSpeed = speed;
        }
        a[3] = currentSpeed;
    }
    if(maxSpeed < currentSpeed) {
        maxSpeed = currentSpeed;
    }
    prev_x = x, prev_y = y, prev_t = t;
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
    rc = sqlite3_exec(pathsdb, "select unitx, unity, time from track_path", callback, NULL, &error_msg);
    cout << rc;
    cout << "\n";
    sqlite3_close(pathsdb);
    alpha = 0;
	counter = 0;
	cout << "About to load the font";
    cout << "\n";
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
    int prevX = points[pathLength - 1][0]*zoom;
    int prevY = points[pathLength - 1][1]*zoom;
    int speed;
    fbo.begin();
    ofSetColor(255, 255, 255);
    for(unsigned int i=max(0, int(pathLength - 40)); i<pathLength; i++) {
        speed = abs(points[i][3]);
        cout << "speed: ";
        cout << speed;
        cout << "\n";
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
        cout << R;
        cout << ", ";
        cout << G;
        cout << ", ";
        cout << B;
        cout << "\n";
        for(int i = 0; i < pointHeight; i++) {
            for(int j = 0; j < pointWidth; j++) {
                colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3];
                colorAlphaPixels[(j*pointWidth+i)*4 + 0] = R;
                colorAlphaPixels[(j*pointWidth+i)*4 + 1] = G;
                colorAlphaPixels[(j*pointWidth+i)*4 + 2] = B;
            }
        }
        texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
        int X = points[i][0]*zoom, Y = points[i][1]*zoom;
        texPoint.draw(X+viewCoords[0], Y+viewCoords[1], pointWidth, pointHeight);
        prevX = X;
        prevY = Y;
    }
    fbo.end();
    ofDisableAlphaBlending();
    fbo.draw(0, 0);
    ofEnableAlphaBlending();
    ofSetHexColor(0xffffff);
    sprintf(speedString, "Speed: %d", speed);
//    timeFont.drawString(speedString, windowDimensions.x - 250, windowDimensions.y - 10);
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 0] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 1] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 2] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3]*20;
        }
    }
    texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
    int X = points[pathLength - 1][0]*zoom, Y = points[pathLength - 1][1]*zoom;
    if(abs(X - prevX) < 7*zoom && abs(Y - prevY) < 7*zoom) { //Filter out aberrations
        texPoint.draw(X+viewCoords[0], Y+viewCoords[1], pointWidth, pointHeight);
    }
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3]/20;
        }
    }
    time_t milliseconds = points[pathLength][2];
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
