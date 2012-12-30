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

#define pointsPerFrame 20
#define initialPointDiameter 10

struct point {
    int unitx;
    int unity;
    float lat;
    float lon;
    int time;
    float speed;
    ofColor color;
    int tile; // Indicates which tile this point belongs to
};

struct tile {
    bool active;
    ofFbo fbo;
    ofPoint position;
};

vector<point> points;
vector<tile> tiles;
vector<int> activeTiles;
vector<int> activeTilesBefore;
bool dragging = false, stopped = false;
float zoom = 1;
unsigned int pathLength = 0;
int pointWidth = initialPointDiameter*zoom;
int pointHeight = pointWidth;
int pointWidthHalf = pointWidth/2;
int pointHeightHalf = pointHeight/2;
unsigned char * colorAlphaPixels, * colorAlphaPixelsHead;
ofTexture texPoint, texHead;
ofTrueTypeFont 	speedFont, timeFont;
MapGeo prev_lon = 0, prev_lat = 0;
int prev_t = 0;
int currentSpeed = 0;
float maxSpeed = 0, maxSpeedHalf, maxSpeedQuarter;
ofPoint initialCursorPosition;
ofPoint windowDimensions;
ofPoint viewCoords; // Will indicate the coordinates of the current viewport, relative to the initial point
ofPoint viewCoordsBeforeDrag;

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
    newPoint.unitx = (x - 287990000)/380;
    newPoint.unity = (y - 175950000)/380;
    /* These coordinates center the visualization on Barcelona
    newPoint.unitx = (atoi(argv[0]) - 271380000)/380;
    newPoint.unity = (atoi(argv[1]) - 200380000)/380; */
    MapGeo lat;
    MapGeo lon;
    unit2latlon_google(x, y, &lat, &lon);
    double distance = latlon2distance(lat, lon, prev_lat, prev_lon);
    int speed = (int)((double)(distance*3600000)/(double)(max(1, t - prev_t)));
    if(strcmp(argv[0], "0") != 0){
        newPoint.time = t;
        newPoint.speed = currentSpeed;
        if(speed >= 0) {
            currentSpeed = speed;
        }
        if(tiles.size() == 0) { // It's the first point; create the first tile for it
            ofFbo fbo;
            fbo.allocate(windowDimensions.x, windowDimensions.y, GL_RGBA);
            tile newTile;
            newTile.active = true;
            newTile.fbo = fbo;
            newTile.position.x = newPoint.unitx - 0.5*windowDimensions.x;
            newTile.position.y = newPoint.unity - 0.5*windowDimensions.y;
            tiles.push_back(newTile);
            newPoint.tile = 0;
        }
        else { // We have to check whether the point belongs to any already existing tile
            int *minimumTilesOfDistance = new int[2];
            int i = 0;
            bool inTiles = false;
            while(i<tiles.size() && !inTiles) {
                //if(newPoint.unitx > tiles[i].position.x && newPoint.unitx < tiles[i].position.x + windowDimensions.x && newPoint.unity > tiles[i].position.y && newPoint.unity < tiles[i].position.y + windowDimensions.y) {
                ofPoint distanceToTile;
                distanceToTile.set(newPoint.unitx - tiles[i].position.x, newPoint.unity - tiles[i].position.y);
                if(distanceToTile.x >= 0 && distanceToTile.x < windowDimensions.x && distanceToTile.y >= 0 && distanceToTile.y < windowDimensions.y) {
                    //cout << "distanceToTile.x: " << distanceToTile.x << "\n";
                    newPoint.tile = i; // The point belongs to this tile
                    inTiles = true;
                    //cout << "point in tile: " << i << "\n";
                }
                else {
                    int *tilesOfDistance = new int[2];
                    tilesOfDistance[0] = distanceToTile.x / int(windowDimensions.x);
                    if(distanceToTile.x < 0) {
                        tilesOfDistance[0]--;
                    }
                    tilesOfDistance[1] = distanceToTile.y / int(windowDimensions.y);
                    if(distanceToTile.y < 0) {
                        tilesOfDistance[1]--;
                    }
                    //cout << "tilesOfDistance[0]: " << tilesOfDistance[1] << "\n";
                    if(i == 0 || abs(minimumTilesOfDistance[0]) + abs(minimumTilesOfDistance[1]) > abs(tilesOfDistance[0]) + abs(tilesOfDistance[1])) {
                        newPoint.tile = i;
                        minimumTilesOfDistance = tilesOfDistance;
                        //cout << "tilesOfDistance[0]: " << tilesOfDistance[0] << "\n";
                        //cout << "minimumTilesOfDistance[0]: " << minimumTilesOfDistance[0] << "\n";
                        //cout << "tilesOfDistance: " << minimumTilesOfDistance[0] << ", " << minimumTilesOfDistance[1] << "\n";
                    }
                    i++;
                }
            }
            if(!inTiles) {
                /*cout << "minimumTilesOfDistance: " << minimumTilesOfDistance[0] << ", " << minimumTilesOfDistance[1] << "\n";
                cout << "distance: " << minimumTilesOfDistance[0]*windowDimensions.x << ", " << minimumTilesOfDistance[1]*windowDimensions.y << "\n";
                cout << "point not in any tile\n";*/
                if(tiles.size() < 100) { // FIXME: rude way to prevent too many tiles from being created
                    ofFbo fbo;
                    fbo.allocate(windowDimensions.x, windowDimensions.y, GL_RGBA);
                    tile newTile;
                    newTile.active = false;
                    newTile.fbo = fbo;
                    cout << "newPoint: " << newPoint.unitx << ", " << newPoint.unity << "\n";
                    /*cout << "minimumTilesOfDistance[0]*windowDimensions.x: " << (minimumTilesOfDistance[0])*(windowDimensions.x) << "\n\n\n";*/
                    newTile.position.x = tiles[newPoint.tile].position.x + minimumTilesOfDistance[0]*windowDimensions.x;
                    newTile.position.y = tiles[newPoint.tile].position.y + minimumTilesOfDistance[1]*windowDimensions.y;
                    cout << "New tile " << tiles.size() << "; " << newTile.position.x << ", " << newTile.position.y << "\n";
                    cout << "minimumTilesOfDistance: " << minimumTilesOfDistance[0] << ", " << minimumTilesOfDistance[1] << " to tile " << newPoint.tile << "\n";
                    tiles.push_back(newTile);
                    newPoint.tile = tiles.size();
                }
            }
        }
        points.push_back(newPoint);
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

void updateActiveTiles() {
    activeTilesBefore = activeTiles;
    activeTiles.clear();
    for(int i=0; i<tiles.size(); i++) {
        /*cout << "tiles[" << i <<"].position.x: " << tiles[i].position.x<< "\n";
        cout << "viewCoords.x: " << viewCoords.x << "\n";
        cout << "tiles[" << i <<"].position.y: " << tiles[i].position.y << "\n";
        cout << "viewCoords.y: " << viewCoords.y << "\n";*/
        int relativeX = tiles[i].position.x - viewCoords.x;
        int relativeY = tiles[i].position.y - viewCoords.y;
        if(relativeX >= - windowDimensions.x && relativeX < windowDimensions.x && relativeY >= - windowDimensions.y && relativeY < windowDimensions.y) {
            cout << "Tile " << i << " is visible; X: " << tiles[i].position.x << "; Y: " << tiles[i].position.y << "\n";
            //cout << "relativeX: " << relativeX << "\n";
            //cout << "relativeY: " << relativeY << "\n";
            tiles[i].active = true;
            activeTiles.push_back(i);
        }
        else {
            //cout << "Tile is not visible: " << i << "\n";
            tiles[i].active = false;
        }
    }
    //cout << "viewCoords: " << viewCoords.x << ", " << viewCoords.y << "\n";
    cout << "Number of visible tiles: " << activeTiles.size() << "\n";
}

void drawPoints(unsigned int from, unsigned int to, unsigned int nTile) {
    while(from < to) {
        point currentPoint = points[from];
        if(currentPoint.tile == nTile) {
            changeColorAlphaPixels(colorAlphaPixels, currentPoint.color);
            texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
            int X = currentPoint.unitx*zoom, Y = currentPoint.unity*zoom;
            ofPoint tilePosition = tiles[nTile].position;
            texPoint.draw(X - tilePosition.x - pointWidthHalf, Y - tilePosition.y - pointHeightHalf, pointWidth, pointHeight);
        }
        from++;
    }
}

void redraw() {
    ofSetColor(255, 255, 255);
    for(int i=0; i<activeTiles.size(); i++) {
        int j = activeTiles[i];
        if(find(activeTilesBefore.begin(), activeTilesBefore.end(), j) == activeTilesBefore.end()) { // If the tile was already active before, there's no need to redraw it
            tile tileToDraw = tiles[j];
            ofPoint relativeTilePosition;
            /*cout << "tiles[j].position.x: " << tiles[j].position.x << "\n";
            cout << "viewCoords.x: " << viewCoords.x << "\n";
            cout << "tiles[j].position.x - viewCoords.x: " << tiles[j].position.x - viewCoords.x << "\n";*/
            relativeTilePosition.set(tileToDraw.position.x - viewCoords.x, tileToDraw.position.y - viewCoords.y);
            //cout << "relativeTilePosition: " << relativeTilePosition.x << ", " << relativeTilePosition.y << "\n";
            //cout << "Drawing tile " << j << "\n";
            if(!dragging && !stopped && pathLength + pointsPerFrame <= points.size()) {
                //cout << "Draw tile " << j << "\n";
                tileToDraw.fbo.begin();
                drawPoints(0, pathLength - 1, j);
                tileToDraw.fbo.end();
            }
            else {
                //cout << "Won't draw tile " << j << "\n";
            }
            ofDisableAlphaBlending();
            tileToDraw.fbo.draw(relativeTilePosition.x, relativeTilePosition.y);
            ofEnableAlphaBlending();
        }
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
    cout << "sqlite3_open returned " << rc << ".\n";
    char *error_msg = NULL;
    cout << "SQLite query about to be executed\n";
    rc = sqlite3_exec(pathsdb, "select unitx, unity, time from track_path", sqliteCallback, NULL, &error_msg);
    cout << "sqlite3_exec returned " << rc << ".\n";
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

	activeTiles.push_back(0);
    viewCoords.x = tiles[0].position.x;
    viewCoords.y = tiles[0].position.y   ;
    viewCoordsBeforeDrag.x = viewCoords.x;
    viewCoordsBeforeDrag.y = viewCoords.y;
	ofEnableAlphaBlending();
    cout << "Calculated maxSpeed: " << maxSpeed << "\n";
    maxSpeed = min(maxSpeed, (float)600); // We assume that we never go faster than 600 km/h, anything higher than that is erroneous data
    cout << "maxSpeed: ";
    cout << maxSpeed;
    cout << "\n";
	maxSpeedHalf = maxSpeed/2;
	maxSpeedQuarter = maxSpeedHalf/2;
	cout << "Number of points: " << points.size() << "\n";
    cout << "Number of tiles: " << tiles.size() << "\n";
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
    ofSetColor(255, 255, 255);
    //cout << "activeTiles.size(): " << activeTiles.size() << "\n";
    unsigned int nActiveTiles = activeTiles.size();
    if(activeTiles.size() == 0) {
        updateActiveTiles();
    }
    for(unsigned int i=0; i<activeTiles.size(); i++) {
        int j = activeTiles[i];
        ofPoint relativeTilePosition;
        /*cout << "tiles[j].position.x: " << tiles[j].position.x << "\n";
        cout << "viewCoords.x: " << viewCoords.x << "\n";
        cout << "tiles[j].position.x - viewCoords.x: " << tiles[j].position.x - viewCoords.x << "\n";*/
        //cout << "relativeTilePosition: " << relativeTilePosition.x << ", " << relativeTilePosition.y << "\n";
        //cout << "Drawing tile " << j << "\n";
        if(!dragging && !stopped && pathLength + pointsPerFrame <= points.size()) {
            tiles[j].fbo.begin();
            drawPoints(max(0, int(pathLength - pointsPerFrame)), pathLength - 1, j);
            tiles[j].fbo.end();
        }
        relativeTilePosition.set(tiles[j].position.x - viewCoords.x, tiles[j].position.y - viewCoords.y);
        ofDisableAlphaBlending();
        tiles[j].fbo.draw(relativeTilePosition.x, relativeTilePosition.y);
        ofEnableAlphaBlending();
    }
    if(pathLength + pointsPerFrame <= points.size()) {
        int X = points[pathLength - 1].unitx*zoom, Y = points[pathLength - 1].unity*zoom;
        if(abs(X - prevX) < 7*zoom && abs(Y - prevY) < 7*zoom) { //Filter out aberrations
            bool bUpdateActiveTiles = false;
            if(X < viewCoords.x) {
                viewCoords.x = X;
                bUpdateActiveTiles = true;
            }
            else if(X >= viewCoords.x + windowDimensions.x) {
                viewCoords.x = X - windowDimensions.x - 1;
                bUpdateActiveTiles = true;
            }
            if(Y < viewCoords.y) {
                viewCoords.y = Y;
                bUpdateActiveTiles = true;
            }
            else if(Y >= viewCoords.y + windowDimensions.y) {
                viewCoords.y = Y - windowDimensions.y - 1;
                bUpdateActiveTiles = true;
            }
            if(bUpdateActiveTiles) {
                cout << "viewCoords.x: " << viewCoords.x << " <= X: " << X << " < viewCoords.x + windowDimensions.x: " << viewCoords.x + windowDimensions.x << "\n";
                cout << "viewCoords.y: " << viewCoords.y << " <= Y: " << Y << " < viewCoords.y + windowDimensions.y: " << viewCoords.y + windowDimensions.y << "\n";
                updateActiveTiles();
            }
            texHead.draw(X - viewCoords.x - pointWidthHalf, Y - viewCoords.y - pointHeightHalf, pointWidth, pointHeight);
        }
        time_t milliseconds = points[pathLength].time;
        tm time = *localtime(&milliseconds);
        strftime(eventTimeString, 100, "%d\/%m\/%Y", &time);
    }
    //sprintf(speedString, "%i km/h", speed);
    //speedFont.drawString(speedString, windowDimensions.x - 420, windowDimensions.y - 40);
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
        //viewCoords.x *= 1.2;
        //viewCoords.y *= 1.2;
        updateActiveTiles();
        redraw();
    }
    else if(key == 45) {
        zoom *= 0.8;
        pointWidth = max(float(2), initialPointDiameter*zoom); // At least 3, so that it doesn't disappear
        pointHeight = pointWidth;
        rescalePoint();
        //viewCoords.x *= 0.8;
        //viewCoords.y *= 0.8;
        updateActiveTiles();
        redraw();
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
    if(!dragging) {
        initialCursorPosition.set(x, y);
        dragging = true;
    }
    viewCoords.x = viewCoordsBeforeDrag.x - (x - initialCursorPosition.x); // Amount of displacement that we should apply to the viewport
    viewCoords.y = viewCoordsBeforeDrag.y - (y - initialCursorPosition.y);
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
    if(dragging) {
        dragging = false;
        viewCoordsBeforeDrag.x = viewCoords.x;
        viewCoordsBeforeDrag.y = viewCoords.y;
        updateActiveTiles();
        redraw();
    }
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
