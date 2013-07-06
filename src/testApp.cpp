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
#include "ofxXmlSettings.h"
#define animate true
#define initialPointDiameter 13
#define headTail 20
#define eyeDistance 310
#define initialZoom 1/eyeDistance


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
    ofPoint coordinates;
};

unsigned int pointsPerFrame = 100;
vector<point> points;
vector<tile> tiles;
vector<int> activeTiles;
vector<int> activeTilesBefore;
ofxXmlSettings xmlData;
bool dragging = false, stopped = false, imagesSaved = false;
float relativeZoom = 1;
unsigned int pathLength = 0;
int pointWidth = initialPointDiameter*relativeZoom;
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
ofPoint tileDimensions;
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

void newPoint(int x, int y, int t, double distance) {
    //cout << "distance: " << distance << "\n";
    point newPoint;
    newPoint.unitx = x*initialZoom;
    newPoint.unity = y*initialZoom;
    //cout << "newPoint.unitx: " << newPoint.unitx << "\n";
    //cout << "newPoint.unity: " << newPoint.unity << "\n";
    int speed = (int)((double)(distance*3600000)/(double)(max(1, t - prev_t)));
    //cout << "t - prev_t: " << t - prev_t << "\n";
    newPoint.time = t;
    newPoint.speed = currentSpeed;
    //cout << "newPoint.speed: " << newPoint.speed << "\n";
    if(speed >= 0) {
        currentSpeed = speed;
    }
    if(tiles.size() == 0) { // It's the first point; create the first tile for it
        ofFbo fbo;
        fbo.allocate(tileDimensions.x, tileDimensions.y, GL_RGB);
        tile newTile;
        newTile.active = true;
        newTile.fbo = fbo;
        newTile.coordinates.x = 0;
        newTile.coordinates.y = 0;
        newTile.position.x = newPoint.unitx - 0.5*tileDimensions.x;
        newTile.position.y = newPoint.unity - 0.5*tileDimensions.y;
        tiles.push_back(newTile);
        newPoint.tile = 0;
    }
    else { // We have to check whether the point belongs to any already existing tile
        int *minimumTilesOfDistance = new int[2];
        int i = 0;
        bool inTiles = false;
        while(i<tiles.size() && !inTiles) {
            //if(newPoint.unitx > tiles[i].position.x && newPoint.unitx < tiles[i].position.x + tileDimensions.x && newPoint.unity > tiles[i].position.y && newPoint.unity < tiles[i].position.y + tileDimensions.y) {
            ofPoint distanceToTile;
            distanceToTile.set(newPoint.unitx - tiles[i].position.x, newPoint.unity - tiles[i].position.y);
            if(distanceToTile.x >= 0 && distanceToTile.x < tileDimensions.x && distanceToTile.y >= 0 && distanceToTile.y < tileDimensions.y) {
                //cout << "distanceToTile.x: " << distanceToTile.x << "\n";
                newPoint.tile = i; // The point belongs to this tile
                inTiles = true;
                //cout << "point in tile: " << i << "\n";
            }
            else {
                int *tilesOfDistance = new int[2];
                tilesOfDistance[0] = distanceToTile.x / int(tileDimensions.x);
                if(distanceToTile.x < 0) {
                    tilesOfDistance[0]--;
                }
                tilesOfDistance[1] = distanceToTile.y / int(tileDimensions.y);
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
            cout << "distance: " << minimumTilesOfDistance[0]*tileDimensions.x << ", " << minimumTilesOfDistance[1]*tileDimensions.y << "\n";
            cout << "point not in any tile\n";*/
            if(tiles.size() < 500) { // Kind of rude way to prevent too many tiles from being created --but if this happens, something is probably wrong --or you travel way much :-)
                ofFbo fbo;
                fbo.allocate(tileDimensions.x, tileDimensions.y, GL_RGB);
                tile newTile;
                newTile.active = false;
                newTile.fbo = fbo;
                //cout << "newPoint: " << newPoint.unitx << ", " << newPoint.unity << "\n";
                /*cout << "minimumTilesOfDistance[0]*tileDimensions.x: " << (minimumTilesOfDistance[0])*(tileDimensions.x) << "\n\n\n";*/
                newTile.coordinates.x = tiles[newPoint.tile].coordinates.x + minimumTilesOfDistance[0];
                newTile.coordinates.y = tiles[newPoint.tile].coordinates.y + minimumTilesOfDistance[1];
                newTile.position.x = tiles[newPoint.tile].position.x + minimumTilesOfDistance[0]*tileDimensions.x;
                newTile.position.y = tiles[newPoint.tile].position.y + minimumTilesOfDistance[1]*tileDimensions.y;
                //cout << "New tile " << tiles.size() << "; " << newTile.position.x << ", " << newTile.position.y << "\n";
                //cout << "minimumTilesOfDistance: " << minimumTilesOfDistance[0] << ", " << minimumTilesOfDistance[1] << " to tile " << newPoint.tile << "\n";
                cout << ".";
                tiles.push_back(newTile);
                newPoint.tile = tiles.size();
            }
            else {
                cout << "Too many tiles. Not creating any more tiles\n";
            }
        }
    }
    points.push_back(newPoint);
    if(maxSpeed < currentSpeed) {
        maxSpeed = currentSpeed;
    }
}

static int sqliteCallback(void *NotUsed, int argc, char **argv, char **azColName){
    if(strcmp(argv[0], "0") != 0) {
        int x = atoi(argv[0]), y = atoi(argv[1]), t = atoi(argv[2]);
        MapGeo lat;
        MapGeo lon;
        unit2latlon_google(x, y, &lat, &lon);
        double distance = latlon2distance(lat, lon, prev_lat, prev_lon);
        prev_lat = lat;
        prev_lon = lon;
        newPoint(x, y, t, distance);
        prev_t = t;
        return 0;
    }
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
            //cout << "Tile " << i << " is visible; X: " << tiles[i].position.x << "; Y: " << tiles[i].position.y << "\n";
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
    //cout << "Number of visible tiles: " << activeTiles.size() << "\n";
}

void drawPoints(unsigned int from, unsigned int to, unsigned int nTile) {
    while(from < to) {
        point currentPoint = points[from];
        if(currentPoint.tile == nTile) {
            changeColorAlphaPixels(colorAlphaPixels, currentPoint.color);
            texPoint.loadData(colorAlphaPixels, pointWidth, pointHeight, GL_RGBA);
            int X = currentPoint.unitx*relativeZoom, Y = currentPoint.unity*relativeZoom;
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
	tileDimensions = windowDimensions; // We make the tiles of the same size as the viewport, but may not be necessarily like this
    int rc;
    cout << "Preprocessing...\n";
    sqlite3 *pathsdb; // "Paths" Database Handler
    rc = sqlite3_open("data/paths.db", &pathsdb);
    cout << "sqlite3_open returned " << rc << ".\n";
    char *error_msg = NULL;
    cout << "SQLite query about to be executed";
    rc = sqlite3_exec(pathsdb, "select unitx, unity, time from track_path", sqliteCallback, NULL, &error_msg);
    cout << "sqlite3_exec returned " << rc << ".";
    sqlite3_close(pathsdb);
    cout << "\n";
    ofDirectory gpxDir("./gpx");
    gpxDir.allowExt(""); // Dummy way of only allowing directories
    gpxDir.listDir();
    gpxDir.sort();
    for(int i = 0; i < gpxDir.numFiles(); i++) {
        string dirPath = gpxDir.getPath(i);
        ofDirectory dir(dirPath);
        dir.allowExt("gpx");
        dir.listDir();
        //cout << "dir.numFiles(): " + dir.numFiles() + "\n";
        for(int i = 0; i < dir.numFiles(); i++) {
            string filePath = dir.getPath(i);
            if(xmlData.loadFile(filePath)) {
                //cout << "loaded XML file\n";
                if(xmlData.pushTag("gpx")) {
                    //cout << "gpx tag found\n";
                    if(xmlData.pushTag("trk")) {
                        //cout << "trk tag found\n";
                        if(xmlData.pushTag("trkseg")) {
                            //cout << "trkseg tag found\n";
                            int nPoints = xmlData.getNumTags("trkpt");
                            for(int i = 0; i < nPoints; i = i+5) {
                                double lat = xmlData.getAttribute("trkpt", "lat", 0.0);
                                double lon = xmlData.getAttribute("trkpt", "lon", 0.0);
                                xmlData.removeTag("trkpt"); // Using the getAttribute method without index and removing every tag we read seems a lot faster than using an index in the getAttribute method
                                xmlData.removeTag("trkpt"); // Using the getAttribute method without index and removing every tag we read seems a lot faster than using an index in the getAttribute method
                                xmlData.removeTag("trkpt"); // Using the getAttribute method without index and removing every tag we read seems a lot faster than using an index in the getAttribute method
                                xmlData.removeTag("trkpt"); // Using the getAttribute method without index and removing every tag we read seems a lot faster than using an index in the getAttribute method
                                xmlData.removeTag("trkpt"); // Using the getAttribute method without index and removing every tag we read seems a lot faster than using an index in the getAttribute method
                                //cout << "lat: " << lat << "\n";
                                //cout << "lon: " << lon << "\n";
                                int x, y;
                                //cout << "x: " << x << "\n";
                                //cout << "y: " << y << "\n";
                                latlon2unit_google(lat, lon, &x, &y);
                                string time_str = xmlData.getValue("trkpt:time", "0");
                                char *cstr = new char[time_str.length() + 1];
                                strcpy(cstr, time_str.c_str());
                                struct tm tm = { 0 };
                                strptime(cstr, "%FT%T%z", &tm);
                                delete [] cstr;
                                int t = (int) mktime(&tm);
                                double distance = latlon2distance(lat, lon, prev_lat, prev_lon);
                                prev_lat = lat;
                                prev_lon = lon;
                                newPoint(x, y, t, distance);
                                prev_t = t;
                            }
                        }
                    }
                };
            };
        }
    }
    cout << "Proprocessing done\n";
    alpha = 0;
	counter = 0;
	//cout << "About to load the font";
    //cout << "\n";
	//speedFont.loadFont("DejaVuSans-ExtraLight.ttf", 20);
	timeFont.loadFont("TerminusMedium-4.38.ttf", 20);
	//cout << "Just loaded the font";
    //cout << "\n";

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
        pathLength = min(pathLength + pointsPerFrame, (unsigned int)points.size() - 1);
    }
}

//--------------------------------------------------------------
void testApp::draw(){
    //cout << "pathLength: " << pathLength << "\n";
    int prevX = points[pathLength - 1].unitx*relativeZoom;
    int prevY = points[pathLength - 1].unity*relativeZoom;
    //cout << "activeTiles.size(): " << activeTiles.size() << "\n";
    unsigned int nActiveTiles = activeTiles.size();
    if(nActiveTiles == 0) {
        updateActiveTiles();
    }
    for(unsigned int i=0; i<nActiveTiles; i++) {
        ofSetColor(255, 255, 255);
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
        if(animate) {
            int from = max(0, int(pathLength - headTail));
            for(int nPoint = from; nPoint < pathLength; nPoint++) {
                ofSetColor(255, 255, 255, 31*(nPoint - from)/(pathLength - from));
                point currentPoint = points[nPoint];
                if(currentPoint.tile == j) {
                    int X = currentPoint.unitx*relativeZoom, Y = currentPoint.unity*relativeZoom;
                    texHead.draw(X - viewCoords.x - pointWidthHalf, Y - viewCoords.y - pointHeightHalf, pointWidth, pointHeight);
                }
            }
        }
    }
    if(pathLength + pointsPerFrame <= points.size()) {
        //cout << "Going to draw\n";
        int X = points[pathLength - 1].unitx*relativeZoom, Y = points[pathLength - 1].unity*relativeZoom;
        if(abs(X - prevX) < 7*relativeZoom && abs(Y - prevY) < 7*relativeZoom) { //Filter out aberrations
            bool bUpdateActiveTiles = false;
            bool reCenter = false;
            int relativeX = X - viewCoords.x;
            if(relativeX < 0) {
                if(relativeX < - windowDimensions.x) {
                    reCenter = true;
                }
                else {
                    viewCoords.x = X;
                }
                bUpdateActiveTiles = true;
            }
            else if(relativeX >= windowDimensions.x) {
                if(relativeX >= windowDimensions.x*2) {
                    reCenter = true;
                }
                else {
                    viewCoords.x = X - windowDimensions.x - 1;
                }
                bUpdateActiveTiles = true;
            }
            int relativeY = Y - viewCoords.y;
            if(relativeY < 0) {
                if(relativeY < - windowDimensions.y) {
                    reCenter = true;
                }
                else {
                    viewCoords.y = Y;
                }
                bUpdateActiveTiles = true;
            }
            else if(Y >= viewCoords.y + windowDimensions.y) {
                if(relativeY >= windowDimensions.y*2) {
                    reCenter = true;
                }
                else {
                    viewCoords.y = Y - windowDimensions.y - 1;
                }
                bUpdateActiveTiles = true;
            }
            if(bUpdateActiveTiles) {
                //cout << "viewCoords.x: " << viewCoords.x << " <= X: " << X << " < viewCoords.x + windowDimensions.x: " << viewCoords.x + windowDimensions.x << "\n";
                //cout << "viewCoords.y: " << viewCoords.y << " <= Y: " << Y << " < viewCoords.y + windowDimensions.y: " << viewCoords.y + windowDimensions.y << "\n";
                if(reCenter) {
                    viewCoords.x = X - windowDimensions.x*0.5;
                    viewCoords.y = Y - windowDimensions.y*0.5;
                }
                updateActiveTiles();
            }
            if(animate) {
                ofSetColor(255, 255, 255, 63);
                texHead.draw(X - viewCoords.x - pointWidth, Y - viewCoords.y - pointHeight, pointWidth*2, pointHeight*2);
            }
        }
        if(animate) {
            time_t milliseconds = points[pathLength].time;
            tm time = *localtime(&milliseconds);
            strftime(eventTimeString, 100, "%d\/%m\/%Y", &time);
        }
    }
    else {
        if(!imagesSaved) {
            std::stringstream dirPath;
            float ratio = (float)relativeZoom*eyeDistance;
            cout << "ratio: " << ratio << "\n";
            dirPath << "tiles_" << ratio << "_" << initialPointDiameter;
            if(!ofDirectory::doesDirectoryExist(dirPath.str(), true)) {
                ofDirectory::createDirectory(dirPath.str(), true);
            }
            cout << "dirPath: " << dirPath.str() << "\n";
            unsigned int nTiles = tiles.size();
            //cout << "nTiles: " << nTiles << "\n";
            for(unsigned int i=0; i < nTiles; i++) {
                //cout << "i: " << i << "\n";
                ofImage img;
                tile t = tiles[i];
                ofPixels pixels;
                t.fbo.readToPixels(pixels);
                std::stringstream filename;
                filename << dirPath.str() << "/tile[" << t.coordinates.x << "," << t.coordinates.y << "].png";
                cout << "Writing file: " << filename.str() << "\n";
                ofSaveImage(pixels, filename.str());
            }
            imagesSaved = true;
            cout << "Done.\n";
        }
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
        relativeZoom *= 1.2;
        pointWidth = max(float(2), initialPointDiameter*relativeZoom);
        pointHeight = pointWidth;
        rescalePoint();
        //viewCoords.x *= 1.2;
        //viewCoords.y *= 1.2;
        updateActiveTiles();
        redraw();
    }
    else if(key == 45) {
        relativeZoom *= 0.8;
        pointWidth = max(float(2), initialPointDiameter*relativeZoom); // At least 3, so that it doesn't disappear
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
