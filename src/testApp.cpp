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

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    int *a = new int[2];
    a[0] = (atoi(argv[0]) - 288240000)/150;
    a[1] = (atoi(argv[1]) - 175920000)/150;
    if(strcmp(argv[0], "0") != 0 && a[0] < 2048 && a[1] > 0 && a[1] < 2048){
        points.push_back(a);
/*        cout << argv[0];
        cout << ", ";
        cout << argv[1];
        cout << "\n";*/
    }
    a0 = a;
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
    ofSetColor(255, 255, 255, 10);
//    ofSetLineWidth(400);
    ofEnableBlendMode(OF_BLENDMODE_ADD);
/*    rainbow.loadImage("point.png");
    rainbow.allocate(20, 20, OF_IMAGE_COLOR_ALPHA);*/
    pointWidth = 20;
    pointHeight = pointWidth;
    texPoint.allocate(pointWidth, pointHeight, GL_RGBA);
	colorAlphaPixels	= new unsigned char [pointWidth*pointHeight*4];
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 0] = 255;
            colorAlphaPixels[(j*pointWidth+i)*4 + 1] = 0;
            colorAlphaPixels[(j*pointWidth+i)*4 + 2] = 255;
            int distanceX = abs(j - pointWidth/2);
            int distanceY = abs(i - pointHeight/2);
            float distanceToCenter = sqrt(distanceX*distanceX + distanceY*distanceY);
            float relativeDistanceToCenter = min(float(1), distanceToCenter/(pointWidth/2));
            colorAlphaPixels[(j*pointWidth+i)*4 + 3] = 5*(1 - relativeDistanceToCenter);
        }
    }
    texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
    viewCoords[0] = 0;
    viewCoords[1] = 0;
    initViewCoords[0] = 0;
    initViewCoords[1] = 0;
    dragging = false;
	ofEnableAlphaBlending();
	colorChangeStep = int(points.size()/256);
	cout << colorChangeStep;
	cout << "\n";
	cout << "\n";
}


//--------------------------------------------------------------
void testApp::update(){
    for(int i = 0; i < pointHeight; i++) {
        for(int j = 0; j < pointWidth; j++) {
            colorAlphaPixels[(j*pointWidth+i)*4 + 1] = 0;
            colorAlphaPixels[(j*pointWidth+i)*4 + 2] = 255;
        }
    }
}

//--------------------------------------------------------------
void testApp::draw(){
    ofSetHexColor(0xffffff);
    for(unsigned int i=1; i<points.size(); i++) {
        if(i%colorChangeStep == 0) {
            for(int i = 0; i < pointHeight; i++) {
                for(int j = 0; j < pointWidth; j++) {
                    colorAlphaPixels[(j*pointWidth+i)*4 + 1] += 1;
                    colorAlphaPixels[(j*pointWidth+i)*4 + 2] -= 1;
                }
            }
            texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
        }
        if(abs(points[i][0] - points[i-1][0])
< 10 && abs(points[i][1] - points[i-1][1]) < 10) {
//            ofLine(points[i-1][0]+viewCoords[0], points[i-1][1]+viewCoords[1], points[i][0]+viewCoords[0], points[i][1]+viewCoords[1]);
            texPoint.draw(points[i][0]+viewCoords[0], points[i][1]+viewCoords[1], pointWidth, pointHeight);
        }
    }
}


//--------------------------------------------------------------
void testApp::keyPressed  (int key){

}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

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
