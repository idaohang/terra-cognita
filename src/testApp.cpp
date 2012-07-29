#include "testApp.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

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
//unsigned char 	* colorAlphaPixels2 = new unsigned char [4];
ofTexture texPoint;
//ofTexture texMap;
ofFbo fbo;


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
    int *a = new int[2];
    /* These coordinates are for Berlin */
    a[0] = (atoi(argv[0]) - 288240000)/275;
    a[1] = (atoi(argv[1]) - 175920000)/275;
    /* These coordinates are for Barcelona
    a[0] = (atoi(argv[0]) - 271380000)/250;
    a[1] = (atoi(argv[1]) - 200380000)/250; */
    if(strcmp(argv[0], "0") != 0 && a[0] < 3500 && a[1] > 0 && a[1] < 3500){
        points.push_back(a);
/*        cout << argv[0];
        cout << ", ";
        cout << argv[1];
        cout << "\n";*/
    }
//    a0 = a;
    return 0;
}

//--------------------------------------------------------------
void testApp::setup(){
    int rc;
    cout << "The program is about to start\n";
    sqlite3 *pathsdb; // "Paths" Database Handler
    rc = sqlite3_open("data/paths.db", &pathsdb);
    cout << rc;
    cout << "\n";
    char *error_msg = NULL;
    cout << "SQLite query about to be executed\n";
    rc = sqlite3_exec(pathsdb, "select unitx, unity from track_path", callback, NULL, &error_msg);
    cout << rc;
    cout << "\n";
    sqlite3_close(pathsdb);
    alpha = 0;
	counter = 0;

    ofBackground(0,0,0);
//    ofSetLineWidth(400);
    ofEnableBlendMode(OF_BLENDMODE_ADD);
/*    rainbow.loadImage("point.png");
    rainbow.allocate(20, 20, OF_IMAGE_COLOR_ALPHA);
    cout << "pointWidth: ";
    cout << pointWidth;
    cout << "\n";
    cout << "pointHeight: ";
    cout << pointHeight;
    cout << "\n";*/
    texPoint.allocate(400, 400, GL_RGBA);
//    texMap.allocate(3500, 3500, GL_RGBA);
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
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = 1.5*(1 - relativeDistanceToCenter);
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
}


//--------------------------------------------------------------
void testApp::update(){
//    texMap.loadScreenData(viewCoords[0],viewCoords[1], 3500, 3500);
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
    int prevX = points[0][0]*zoom, prevY = points[0][1]*zoom;
    int R = colorAlphaPixels[0];
    int G = colorAlphaPixels[1];
    int B = colorAlphaPixels[2];
    prevX = points[pathLength - 1][0]*zoom;
    prevY = points[pathLength - 1][1]*zoom;
//    texMap.draw(0, 0);
    fbo.begin();
    ofSetColor(255, 255, 255);
    for(unsigned int i=pathLength - 80; i<pathLength; i++) {
        int X = points[i][0]*zoom, Y = points[i][1]*zoom;
        texPoint.draw(X+viewCoords[0], Y+viewCoords[1], pointWidth, pointHeight);
        //ofCircle(X+viewCoords[0], Y+viewCoords[1], 1);
    }
    fbo.end();
    ofDisableAlphaBlending();
    fbo.draw(0, 0);
    ofEnableAlphaBlending();
    ofSetHexColor(0xffffff);
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3]*6;
        }
    }
    for(unsigned int i=pathLength - 40; i<pathLength; i++) {
        if((i > pathLength - 1500 || i > pathLength/2 && pathLength <= 1500) && i%colorChangeStep == 0 && i != 0) {
/*            cout << "i:";
            cout << i;
            cout << "\n";*/
            R = max(0, R - 1);
            G = min(255, G + 1);
            B = min(192, B + 1);
/*            cout << R;
            cout << "\n";
            cout << G;
            cout << "\n";
            cout << B;
            cout << "\n";*/
            for(int i = 0; i < pointHeight; i++) {
                for(int j = 0; j < pointWidth; j++) {
                    colorAlphaPixels[(j*pointWidth+i)*4 + 0] = R;
                    colorAlphaPixels[(j*pointWidth+i)*4 + 1] = G;
                    colorAlphaPixels[(j*pointWidth+i)*4 + 2] = B;
                }
            }
            texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
        }
        int X = points[i][0]*zoom, Y = points[i][1]*zoom;
/*        cout << "X: ";
        cout << X;
        cout << "\n";
        cout << "Y: ";
        cout << Y;
        cout << "\n";*/
        texPoint.draw(X+viewCoords[0], Y+viewCoords[1], pointWidth, pointHeight);
//        ofCircle(X+viewCoords[0], Y+viewCoords[1], 10);
        if(abs(X - prevX) < 7*zoom && abs(Y - prevY) < 7*zoom) { //Filter out aberrations
        }
        prevX = X;
        prevY = Y;
    }
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 0] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 1] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 2] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = colorAlphaPixels[(j*pointWidth+i)*4 + 3]/6;
        }
    }
//    texMap.loadScreenData(0, 0, 3500,3500);
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
